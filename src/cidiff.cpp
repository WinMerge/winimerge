#ifdef USE_WINIMERGELIB
#include <Windows.h>
#include "WinIMergeLib.h"
#else
#include "ImgDiffBuffer.hpp"
#endif
#include <iostream>
#include <clocale>

int main(int argc, char* argv[])
{
#ifndef USE_WINIMERGELIB
	CImgDiffBuffer buffer;
#endif
	wchar_t filenameW[2][260];
	const wchar_t *filenames[2] = { filenameW[0], filenameW[1] };

	if (argc < 3)
	{
		std::wcerr << L"usage: cmdidiff image_file1 image_file2" << std::endl;
		exit(1);
	}

	setlocale(LC_ALL, "");

	mbstowcs(filenameW[0], argv[1], strlen(argv[1]) + 1);
	mbstowcs(filenameW[1], argv[2], strlen(argv[2]) + 1);

#ifdef USE_WINIMERGELIB
	IImgMergeWindow *pImgMergeWindow = WinIMerge_CreateWindowless();
	if (pImgMergeWindow)
	{
		if (!pImgMergeWindow->OpenImages(filenames[0], filenames[1]))
		{
			std::wcerr << L"cmdidiff: could not open files. (" << filenameW[0] << ", " << filenameW[1] << L")" << std::endl;
			exit(1);
		}
		pImgMergeWindow->SaveDiffImageAs(1, L"diff.png");
		WinIMerge_DestroyWindow(pImgMergeWindow);
	}
#else
	FreeImage_Initialise();

	if (!buffer.OpenImages(2, filenames))
	{
		std::wcerr << L"cmdidiff: could not open files. (" << filenameW[0] << ", " << filenameW[1] << L")" << std::endl;
		exit(1);
	}

	buffer.CompareImages();
	buffer.SaveDiffImageAs(1, L"diff.png");
	buffer.CloseImages();
#endif

	return 0;
}


