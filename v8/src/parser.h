// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_PARSER_H_
#define V8_PARSER_H_

#include "allocation.h"
#include "ast.h"
#include "preparse-data-format.h"
#include "preparse-data.h"
#include "scopes.h"
#include "preparser.h"

namespace v8 {
namespace internal {

class CompilationInfo;
class FuncNameInferrer;
class ParserLog;
class PositionStack;
class Target;

template <typename T> class ZoneListWrapper;


class FunctionEntry BASE_EMBEDDED {
 public:
  enum {
    kStartPositionIndex,
    kEndPositionIndex,
    kLiteralCountIndex,
    kPropertyCountIndex,
    kLanguageModeIndex,
    kSize
  };

  explicit FunctionEntry(Vector<unsigned> backing)
    : backing_(backing) { }

  FunctionEntry() : backing_() { }

  int start_pos() { return backing_[kStartPositionIndex]; }
  int end_pos() { return backing_[kEndPositionIndex]; }
  int literal_count() { return backing_[kLiteralCountIndex]; }
  int property_count() { return backing_[kPropertyCountIndex]; }
  LanguageMode language_mode() {
    ASSERT(backing_[kLanguageModeIndex] == CLASSIC_MODE ||
           backing_[kLanguageModeIndex] == STRICT_MODE ||
           backing_[kLanguageModeIndex] == EXTENDED_MODE);
    return static_cast<LanguageMode>(backing_[kLanguageModeIndex]);
  }

  bool is_valid() { return !backing_.is_empty(); }

 private:
  Vector<unsigned> backing_;
};


class ScriptDataImpl : public ScriptData {
 public:
  explicit ScriptDataImpl(Vector<unsigned> store)
      : store_(store),
        owns_store_(true) { }

  // Create an empty ScriptDataImpl that is guaranteed to not satisfy
  // a SanityCheck.
  ScriptDataImpl() : owns_store_(false) { }

  virtual ~ScriptDataImpl();
  virtual int Length();
  virtual const char* Data();
  virtual bool HasError();

  void Initialize();
  void ReadNextSymbolPosition();

  FunctionEntry GetFunctionEntry(int start);
  int GetSymbolIdentifier();
  bool SanityCheck();

  Scanner::Location MessageLocation();
  const char* BuildMessage();
  Vector<const char*> BuildArgs();

  int symbol_count() {
    return (store_.length() > PreparseDataConstants::kHeaderSize)
        ? store_[PreparseDataConstants::kSymbolCountOffset]
        : 0;
  }
  // The following functions should only be called if SanityCheck has
  // returned true.
  bool has_error() { return store_[PreparseDataConstants::kHasErrorOffset]; }
  unsigned magic() { return store_[PreparseDataConstants::kMagicOffset]; }
  unsigned version() { return store_[PreparseDataConstants::kVersionOffset]; }

 private:
  Vector<unsigned> store_;
  unsigned char* symbol_data_;
  unsigned char* symbol_data_end_;
  int function_index_;
  bool owns_store_;

  unsigned Read(int position);
  unsigned* ReadAddress(int position);
  // Reads a number from the current symbols
  int ReadNumber(byte** source);

  ScriptDataImpl(const char* backing_store, int length)
      : store_(reinterpret_cast<unsigned*>(const_cast<char*>(backing_store)),
               length / static_cast<int>(sizeof(unsigned))),
        owns_store_(false) {
    ASSERT_EQ(0, static_cast<int>(
        reinterpret_cast<intptr_t>(backing_store) % sizeof(unsigned)));
  }

  // Read strings written by ParserRecorder::WriteString.
  static const char* ReadString(unsigned* start, int* chars);

  friend class ScriptData;
};


class PreParserApi {
 public:
  // Pre-parse a character stream and return full preparse data.
  //
  // This interface is here instead of in preparser.h because it instantiates a
  // preparser recorder object that is suited to the parser's purposes.  Also,
  // the preparser doesn't know about ScriptDataImpl.
  static ScriptDataImpl* PreParse(Isolate* isolate,
                                  Utf16CharacterStream* source);
};


// ----------------------------------------------------------------------------
// REGEXP PARSING

// A BufferedZoneList is an automatically growing list, just like (and backed
// by) a ZoneList, that is optimized for the case of adding and removing
// a single element. The last element added is stored outside the backing list,
// and if no more than one element is ever added, the ZoneList isn't even
// allocated.
// Elements must not be NULL pointers.
template <typename T, int initial_size>
class BufferedZoneList {
 public:
  BufferedZoneList() : list_(NULL), last_(NULL) {}

  // Adds element at end of list. This element is buffered and can
  // be read using last() or removed using RemoveLast until a new Add or until
  // RemoveLast or GetList has been called.
  void Add(T* value, Zone* zone) {
    if (last_ != NULL) {
      if (list_ == NULL) {
        list_ = new(zone) ZoneList<T*>(initial_size, zone);
      }
      list_->Add(last_, zone);
    }
    last_ = value;
  }

  T* last() {
    ASSERT(last_ != NULL);
    return last_;
  }

  T* RemoveLast() {
    ASSERT(last_ != NULL);
    T* result = last_;
    if ((list_ != NULL) && (list_->length() > 0))
      last_ = list_->RemoveLast();
    else
      last_ = NULL;
    return result;
  }

  T* Get(int i) {
    ASSERT((0 <= i) && (i < length()));
    if (list_ == NULL) {
      ASSERT_EQ(0, i);
      return last_;
    } else {
      if (i == list_->length()) {
        ASSERT(last_ != NULL);
        return last_;
      } else {
        return list_->at(i);
      }
    }
  }

  void Clear() {
    list_ = NULL;
    last_ = NULL;
  }

  int length() {
    int length = (list_ == NULL) ? 0 : list_->length();
    return length + ((last_ == NULL) ? 0 : 1);
  }

  ZoneList<T*>* GetList(Zone* zone) {
    if (list_ == NULL) {
      list_ = new(zone) ZoneList<T*>(initial_size, zone);
    }
    if (last_ != NULL) {
      list_->Add(last_, zone);
      last_ = NULL;
    }
    return list_;
  }

 private:
  ZoneList<T*>* list_;
  T* last_;
};


// Accumulates RegExp atoms and assertions into lists of terms and alternatives.
class RegExpBuilder: public ZoneObject {
 public:
  explicit RegExpBuilder(Zone* zone);
  void AddCharacter(uc16 character);
  // "Adds" an empty expression. Does nothing except consume a
  // following quantifier
  void AddEmpty();
  void AddAtom(RegExpTree* tree);
  void AddAssertion(RegExpTree* tree);
  void NewAlternative();  // '|'
  void AddQuantifierToAtom(
      int min, int max, RegExpQuantifier::QuantifierType type);
  RegExpTree* ToRegExp();

 private:
  void FlushCharacters();
  void FlushText();
  void FlushTerms();
  Zone* zone() const { return zone_; }

  Zone* zone_;
  bool pending_empty_;
  ZoneList<uc16>* characters_;
  BufferedZoneList<RegExpTree, 2> terms_;
  BufferedZoneList<RegExpTree, 2> text_;
  BufferedZoneList<RegExpTree, 2> alternatives_;
#ifdef DEBUG
  enum {ADD_NONE, ADD_CHAR, ADD_TERM, ADD_ASSERT, ADD_ATOM} last_added_;
#define LAST(x) last_added_ = x;
#else
#define LAST(x)
#endif
};


class RegExpParser BASE_EMBEDDED {
 public:
  RegExpParser(FlatStringReader* in,
               Handle<String>* error,
               bool multiline_mode,
               Zone* zone);

  static bool ParseRegExp(FlatStringReader* input,
                          bool multiline,
                          RegExpCompileData* result,
                          Zone* zone);

  RegExpTree* ParsePattern();
  RegExpTree* ParseDisjunction();
  RegExpTree* ParseGroup();
  RegExpTree* ParseCharacterClass();

  // Parses a {...,...} quantifier and stores the range in the given
  // out parameters.
  bool ParseIntervalQuantifier(int* min_out, int* max_out);

  // Parses and returns a single escaped character.  The character
  // must not be 'b' or 'B' since they are usually handle specially.
  uc32 ParseClassCharacterEscape();

  // Checks whether the following is a length-digit hexadecimal number,
  // and sets the value if it is.
  bool ParseHexEscape(int length, uc32* value);

  uc32 ParseOctalLiteral();

  // Tries to parse the input as a back reference.  If successful it
  // stores the result in the output parameter and returns true.  If
  // it fails it will push back the characters read so the same characters
  // can be reparsed.
  bool ParseBackReferenceIndex(int* index_out);

  CharacterRange ParseClassAtom(uc16* char_class);
  RegExpTree* ReportError(Vector<const char> message);
  void Advance();
  void Advance(int dist);
  void Reset(int pos);

  // Reports whether the pattern might be used as a literal search string.
  // Only use if the result of the parse is a single atom node.
  bool simple();
  bool contains_anchor() { return contains_anchor_; }
  void set_contains_anchor() { contains_anchor_ = true; }
  int captures_started() { return captures_ == NULL ? 0 : captures_->length(); }
  int position() { return next_pos_ - 1; }
  bool failed() { return failed_; }

  static const int kMaxCaptures = 1 << 16;
  static const uc32 kEndMarker = (1 << 21);

 private:
  enum SubexpressionType {
    INITIAL,
    CAPTURE,  // All positive values represent captures.
    POSITIVE_LOOKAHEAD,
    NEGATIVE_LOOKAHEAD,
    GROUPING
  };

  class RegExpParserState : public ZoneObject {
   public:
    RegExpParserState(RegExpParserState* previous_state,
                      SubexpressionType group_type,
                      int disjunction_capture_index,
                      Zone* zone)
        : previous_state_(previous_state),
          builder_(new(zone) RegExpBuilder(zone)),
          group_type_(group_type),
          disjunction_capture_index_(disjunction_capture_index) {}
    // Parser state of containing expression, if any.
    RegExpParserState* previous_state() { return previous_state_; }
    bool IsSubexpression() { return previous_state_ != NULL; }
    // RegExpBuilder building this regexp's AST.
    RegExpBuilder* builder() { return builder_; }
    // Type of regexp being parsed (parenthesized group or entire regexp).
    SubexpressionType group_type() { return group_type_; }
    // Index in captures array of first capture in this sub-expression, if any.
    // Also the capture index of this sub-expression itself, if group_type
    // is CAPTURE.
    int capture_index() { return disjunction_capture_index_; }

   private:
    // Linked list implementation of stack of states.
    RegExpParserState* previous_state_;
    // Builder for the stored disjunction.
    RegExpBuilder* builder_;
    // Stored disjunction type (capture, look-ahead or grouping), if any.
    SubexpressionType group_type_;
    // Stored disjunction's capture index (if any).
    int disjunction_capture_index_;
  };

  Isolate* isolate() { return isolate_; }
  Zone* zone() const { return zone_; }

  uc32 current() { return current_; }
  bool has_more() { return has_more_; }
  bool has_next() { return next_pos_ < in()->length(); }
  uc32 Next();
  FlatStringReader* in() { return in_; }
  void ScanForCaptures();

  Isolate* isolate_;
  Zone* zone_;
  Handle<String>* error_;
  ZoneList<RegExpCapture*>* captures_;
  FlatStringReader* in_;
  uc32 current_;
  int next_pos_;
  // The capture count is only valid after we have scanned for captures.
  int capture_count_;
  bool has_more_;
  bool multiline_;
  bool simple_;
  bool contains_anchor_;
  bool is_scanned_for_captures_;
  bool failed_;
};

// ----------------------------------------------------------------------------
// JAVASCRIPT PARSING

class Parser;
class SingletonLogger;

class ParserTraits {
 public:
  struct Type {
    typedef v8::internal::Parser* Parser;

    // Types used by FunctionState and BlockState.
    typedef v8::internal::Scope Scope;
    typedef AstNodeFactory<AstConstructionVisitor> Factory;
    typedef Variable GeneratorVariable;
    typedef v8::internal::Zone Zone;

    // Return types for traversing functions.
    typedef Handle<String> Identifier;
    typedef v8::internal::Expression* Expression;
    typedef ZoneList<v8::internal::Expression*>* ExpressionList;
  };

  explicit ParserTraits(Parser* parser) : parser_(parser) {}

  // Custom operations executed when FunctionStates are created and destructed.
  template<typename FS>
  static void SetUpFunctionState(FS* function_state, Zone* zone) {
    Isolate* isolate = zone->isolate();
    function_state->isolate_ = isolate;
    function_state->saved_ast_node_id_ = isolate->ast_node_id();
    isolate->set_ast_node_id(BailoutId::FirstUsable().ToInt());
  }

  template<typename FS>
  static void TearDownFunctionState(FS* function_state) {
    if (function_state->outer_function_state_ != NULL) {
      function_state->isolate_->set_ast_node_id(
          function_state->saved_ast_node_id_);
    }
  }

  // Helper functions for recursive descent.
  bool IsEvalOrArguments(Handle<String> identifier) const;

  // Reporting errors.
  void ReportMessageAt(Scanner::Location source_location,
                       const char* message,
                       Vector<const char*> args);
  void ReportMessage(const char* message, Vector<Handle<String> > args);
  void ReportMessageAt(Scanner::Location source_location,
                       const char* message,
                       Vector<Handle<String> > args);

  // "null" return type creators.
  static Handle<String> EmptyIdentifier() {
    return Handle<String>();
  }
  static Expression* EmptyExpression() {
    return NULL;
  }

  // Odd-ball literal creators.
  Literal* GetLiteralTheHole(int position,
                             AstNodeFactory<AstConstructionVisitor>* factory);

  // Producing data during the recursive descent.
  Handle<String> GetSymbol(Scanner* scanner = NULL);
  Handle<String> NextLiteralString(Scanner* scanner,
                                   PretenureFlag tenured);
  Expression* ThisExpression(Scope* scope,
                             AstNodeFactory<AstConstructionVisitor>* factory);
  Expression* ExpressionFromLiteral(
      Token::Value token, int pos, Scanner* scanner,
      AstNodeFactory<AstConstructionVisitor>* factory);
  Expression* ExpressionFromIdentifier(
      Handle<String> name, int pos, Scope* scope,
      AstNodeFactory<AstConstructionVisitor>* factory);
  Expression* ExpressionFromString(
      int pos, Scanner* scanner,
      AstNodeFactory<AstConstructionVisitor>* factory);
  ZoneList<v8::internal::Expression*>* NewExpressionList(int size, Zone* zone) {
    return new(zone) ZoneList<v8::internal::Expression*>(size, zone);
  }

  // Temporary glue; these functions will move to ParserBase.
  Expression* ParseObjectLiteral(bool* ok);
  Expression* ParseAssignmentExpression(bool accept_IN, bool* ok);
  Expression* ParseV8Intrinsic(bool* ok);

 private:
  Parser* parser_;
};


class Parser : public ParserBase<ParserTraits> {
 public:
  explicit Parser(CompilationInfo* info);
  ~Parser() {
    delete reusable_preparser_;
    reusable_preparser_ = NULL;
  }

  // Parses the source code represented by the compilation info and sets its
  // function literal.  Returns false (and deallocates any allocated AST
  // nodes) if parsing failed.
  static bool Parse(CompilationInfo* info,
                    bool allow_lazy = false) {
    Parser parser(info);
    parser.set_allow_lazy(allow_lazy);
    return parser.Parse();
  }
  bool Parse();

 private:
  friend class ParserTraits;

  static const int kMaxNumFunctionLocals = 131071;  // 2^17-1

  enum Mode {
    PARSE_LAZILY,
    PARSE_EAGERLY
  };

  enum VariableDeclarationContext {
    kModuleElement,
    kBlockElement,
    kStatement,
    kForStatement
  };

  // If a list of variable declarations includes any initializers.
  enum VariableDeclarationProperties {
    kHasInitializers,
    kHasNoInitializers
  };

  class ParsingModeScope BASE_EMBEDDED {
   public:
    ParsingModeScope(Parser* parser, Mode mode)
        : parser_(parser),
          old_mode_(parser->mode()) {
      parser_->mode_ = mode;
    }
    ~ParsingModeScope() {
      parser_->mode_ = old_mode_;
    }

   private:
    Parser* parser_;
    Mode old_mode_;
  };

  // Returns NULL if parsing failed.
  FunctionLiteral* ParseProgram();

  FunctionLiteral* ParseLazy();
  FunctionLiteral* ParseLazy(Utf16CharacterStream* source);

  Isolate* isolate() { return isolate_; }
  CompilationInfo* info() const { return info_; }

  // Called by ParseProgram after setting up the scanner.
  FunctionLiteral* DoParseProgram(CompilationInfo* info,
                                  Handle<String> source);

  // Report syntax error
  void ReportInvalidPreparseData(Handle<String> name, bool* ok);

  void set_pre_parse_data(ScriptDataImpl *data) {
    pre_parse_data_ = data;
    symbol_cache_.Initialize(data ? data->symbol_count() : 0, zone());
  }

  bool inside_with() const { return scope_->inside_with(); }
  Mode mode() const { return mode_; }
  ScriptDataImpl* pre_parse_data() const { return pre_parse_data_; }
  bool is_extended_mode() {
    ASSERT(scope_ != NULL);
    return scope_->is_extended_mode();
  }
  Scope* DeclarationScope(VariableMode mode) {
    return IsLexicalVariableMode(mode)
        ? scope_ : scope_->DeclarationScope();
  }

  // All ParseXXX functions take as the last argument an *ok parameter
  // which is set to false if parsing failed; it is unchanged otherwise.
  // By making the 'exception handling' explicit, we are forced to check
  // for failure at the call sites.
  void* ParseSourceElements(ZoneList<Statement*>* processor, int end_token,
                            bool is_eval, bool is_global, bool* ok);
  Statement* ParseModuleElement(ZoneStringList* labels, bool* ok);
  Statement* ParseModuleDeclaration(ZoneStringList* names, bool* ok);
  Module* ParseModule(bool* ok);
  Module* ParseModuleLiteral(bool* ok);
  Module* ParseModulePath(bool* ok);
  Module* ParseModuleVariable(bool* ok);
  Module* ParseModuleUrl(bool* ok);
  Module* ParseModuleSpecifier(bool* ok);
  Block* ParseImportDeclaration(bool* ok);
  Statement* ParseExportDeclaration(bool* ok);
  Statement* ParseBlockElement(ZoneStringList* labels, bool* ok);
  Statement* ParseStatement(ZoneStringList* labels, bool* ok);
  Statement* ParseFunctionDeclaration(ZoneStringList* names, bool* ok);
  Statement* ParseNativeDeclaration(bool* ok);
  Block* ParseBlock(ZoneStringList* labels, bool* ok);
  Block* ParseVariableStatement(VariableDeclarationContext var_context,
                                ZoneStringList* names,
                                bool* ok);
  Block* ParseVariableDeclarations(VariableDeclarationContext var_context,
                                   VariableDeclarationProperties* decl_props,
                                   ZoneStringList* names,
                                   Handle<String>* out,
                                   bool* ok);
  Statement* ParseExpressionOrLabelledStatement(ZoneStringList* labels,
                                                bool* ok);
  IfStatement* ParseIfStatement(ZoneStringList* labels, bool* ok);
  Statement* ParseContinueStatement(bool* ok);
  Statement* ParseBreakStatement(ZoneStringList* labels, bool* ok);
  Statement* ParseReturnStatement(bool* ok);
  Statement* ParseWithStatement(ZoneStringList* labels, bool* ok);
  CaseClause* ParseCaseClause(bool* default_seen_ptr, bool* ok);
  SwitchStatement* ParseSwitchStatement(ZoneStringList* labels, bool* ok);
  DoWhileStatement* ParseDoWhileStatement(ZoneStringList* labels, bool* ok);
  WhileStatement* ParseWhileStatement(ZoneStringList* labels, bool* ok);
  Statement* ParseForStatement(ZoneStringList* labels, bool* ok);
  Statement* ParseThrowStatement(bool* ok);
  Expression* MakeCatchContext(Handle<String> id, VariableProxy* value);
  TryStatement* ParseTryStatement(bool* ok);
  DebuggerStatement* ParseDebuggerStatement(bool* ok);

  // Support for hamony block scoped bindings.
  Block* ParseScopedBlock(ZoneStringList* labels, bool* ok);

  Expression* ParseAssignmentExpression(bool accept_IN, bool* ok);
  Expression* ParseYieldExpression(bool* ok);
  Expression* ParseConditionalExpression(bool accept_IN, bool* ok);
  Expression* ParseBinaryExpression(int prec, bool accept_IN, bool* ok);
  Expression* ParseUnaryExpression(bool* ok);
  Expression* ParsePostfixExpression(bool* ok);
  Expression* ParseLeftHandSideExpression(bool* ok);
  Expression* ParseMemberWithNewPrefixesExpression(bool* ok);
  Expression* ParseMemberExpression(bool* ok);
  Expression* ParseMemberExpressionContinuation(Expression* expression,
                                                bool* ok);
  Expression* ParseObjectLiteral(bool* ok);

  // Initialize the components of a for-in / for-of statement.
  void InitializeForEachStatement(ForEachStatement* stmt,
                                  Expression* each,
                                  Expression* subject,
                                  Statement* body);

  ZoneList<Expression*>* ParseArguments(bool* ok);
  FunctionLiteral* ParseFunctionLiteral(
      Handle<String> name,
      Scanner::Location function_name_location,
      bool name_is_strict_reserved,
      bool is_generator,
      int function_token_position,
      FunctionLiteral::FunctionType type,
      bool* ok);

  // Magical syntax support.
  Expression* ParseV8Intrinsic(bool* ok);

  bool CheckInOrOf(bool accept_OF, ForEachStatement::VisitMode* visit_mode);

  Handle<String> LiteralString(PretenureFlag tenured) {
    if (scanner()->is_literal_ascii()) {
      return isolate_->factory()->NewStringFromAscii(
          scanner()->literal_ascii_string(), tenured);
    } else {
      return isolate_->factory()->NewStringFromTwoByte(
            scanner()->literal_utf16_string(), tenured);
    }
  }

  // Get odd-ball literals.
  Literal* GetLiteralUndefined(int position);

  // Determine if the expression is a variable proxy and mark it as being used
  // in an assignment or with a increment/decrement operator. This is currently
  // used on for the statically checking assignments to harmony const bindings.
  void MarkAsLValue(Expression* expression);

  // Strict mode validation of LValue expressions
  void CheckStrictModeLValue(Expression* expression,
                             bool* ok);

  // For harmony block scoping mode: Check if the scope has conflicting var/let
  // declarations from different scopes. It covers for example
  //
  // function f() { { { var x; } let x; } }
  // function g() { { var x; let x; } }
  //
  // The var declarations are hoisted to the function scope, but originate from
  // a scope where the name has also been let bound or the var declaration is
  // hoisted over such a scope.
  void CheckConflictingVarDeclarations(Scope* scope, bool* ok);

  // Parser support
  VariableProxy* NewUnresolved(Handle<String> name,
                               VariableMode mode,
                               Interface* interface);
  void Declare(Declaration* declaration, bool resolve, bool* ok);

  bool TargetStackContainsLabel(Handle<String> label);
  BreakableStatement* LookupBreakTarget(Handle<String> label, bool* ok);
  IterationStatement* LookupContinueTarget(Handle<String> label, bool* ok);

  void RegisterTargetUse(Label* target, Target* stop);

  // Factory methods.

  Scope* NewScope(Scope* parent, ScopeType type);

  Handle<String> LookupSymbol(int symbol_id);

  Handle<String> LookupCachedSymbol(int symbol_id);

  // Generate AST node that throw a ReferenceError with the given type.
  Expression* NewThrowReferenceError(Handle<String> type);

  // Generate AST node that throw a SyntaxError with the given
  // type. The first argument may be null (in the handle sense) in
  // which case no arguments are passed to the constructor.
  Expression* NewThrowSyntaxError(Handle<String> type, Handle<Object> first);

  // Generate AST node that throw a TypeError with the given
  // type. Both arguments must be non-null (in the handle sense).
  Expression* NewThrowTypeError(Handle<String> type,
                                Handle<Object> first,
                                Handle<Object> second);

  // Generic AST generator for throwing errors from compiled code.
  Expression* NewThrowError(Handle<String> constructor,
                            Handle<String> type,
                            Vector< Handle<Object> > arguments);

  PreParser::PreParseResult LazyParseFunctionLiteral(
       SingletonLogger* logger);

  Isolate* isolate_;
  ZoneList<Handle<String> > symbol_cache_;

  Handle<Script> script_;
  Scanner scanner_;
  PreParser* reusable_preparser_;
  Scope* original_scope_;  // for ES5 function declarations in sloppy eval
  Target* target_stack_;  // for break, continue statements
  ScriptDataImpl* pre_parse_data_;
  FuncNameInferrer* fni_;

  Mode mode_;

  CompilationInfo* info_;
};


// Support for handling complex values (array and object literals) that
// can be fully handled at compile time.
class CompileTimeValue: public AllStatic {
 public:
  enum LiteralType {
    OBJECT_LITERAL_FAST_ELEMENTS,
    OBJECT_LITERAL_SLOW_ELEMENTS,
    ARRAY_LITERAL
  };

  static bool IsCompileTimeValue(Expression* expression);

  // Get the value as a compile time value.
  static Handle<FixedArray> GetValue(Isolate* isolate, Expression* expression);

  // Get the type of a compile time value returned by GetValue().
  static LiteralType GetLiteralType(Handle<FixedArray> value);

  // Get the elements array of a compile time value returned by GetValue().
  static Handle<FixedArray> GetElements(Handle<FixedArray> value);

 private:
  static const int kLiteralTypeSlot = 0;
  static const int kElementsSlot = 1;

  DISALLOW_IMPLICIT_CONSTRUCTORS(CompileTimeValue);
};

} }  // namespace v8::internal

#endif  // V8_PARSER_H_
