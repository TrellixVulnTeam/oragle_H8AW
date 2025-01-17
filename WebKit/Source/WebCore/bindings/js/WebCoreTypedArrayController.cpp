/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "WebCoreTypedArrayController.h"

#include "JSDOMBinding.h"
#include "JSDOMGlobalObject.h"
#include <runtime/ArrayBuffer.h>
#include <runtime/JSArrayBuffer.h>
#include <runtime/JSCInlines.h>

namespace WebCore {

WebCoreTypedArrayController::WebCoreTypedArrayController()
{
}

WebCoreTypedArrayController::~WebCoreTypedArrayController()
{
}

JSC::JSArrayBuffer* WebCoreTypedArrayController::toJS(JSC::ExecState* state, JSC::JSGlobalObject* globalObject, JSC::ArrayBuffer* buffer)
{
    return JSC::jsCast<JSC::JSArrayBuffer*>(WebCore::toJS(state, JSC::jsCast<JSDOMGlobalObject*>(globalObject), buffer));
}

bool WebCoreTypedArrayController::JSArrayBufferOwner::isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown> handle, void*, JSC::SlotVisitor& visitor)
{
    auto& wrapper = *JSC::jsCast<JSC::JSArrayBuffer*>(handle.get().asCell());
    if (!wrapper.hasCustomProperties())
        return false;
    return visitor.containsOpaqueRoot(wrapper.impl());
}

void WebCoreTypedArrayController::JSArrayBufferOwner::finalize(JSC::Handle<JSC::Unknown> handle, void* context)
{
    auto& wrapper = *static_cast<JSC::JSArrayBuffer*>(handle.get().asCell());
    auto& buffer = *wrapper.impl();
    uncacheWrapper(*static_cast<DOMWrapperWorld*>(context), &buffer, &wrapper);
    buffer.deref();
}

} // namespace WebCore
