#if defined(_WIN32)
#define _CRT_SECURE_NO_DEPRECATE
#endif


#include <string>
#include <iostream>
#include <cstdlib>


int main(int argc, char** argv)
{
	if (argc > 1)
	{
		std::string arg(argv[1]);
		if (arg == "-hello")
		{
			std::cout << "Hello, world!";
		}
		else if (arg == "-count")
		{
			int n = 0;
			int c = std::cin.get();
			while (c != -1) { ++n; c = std::cin.get(); }
			return n;
		}
		else if (arg == "-env")
		{
			const char* s = std::getenv("TESTENV");
			if (s)
			{
				std::cout << s;
				return 0;
			}
			else return 1;
		}
	}
	return argc - 1;
}
