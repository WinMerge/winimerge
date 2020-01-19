#ifdef _WIN64

#include "Win78Libraries.h"

namespace Win78Libraries
{
	CreateRandomAccessStreamOnFileType CreateRandomAccessStreamOnFile;
	WindowsCreateStringReferenceType WindowsCreateStringReference;
	RoGetActivationFactoryType RoGetActivationFactory;
	RoActivateInstanceType RoActivateInstance;
	D2D1CreateFactoryType D2D1CreateFactory;
	HMODULE hLibraryShcore;
	HMODULE hLibraryCombase;
	HMODULE hLibraryD2D1;

	void load()
	{
		hLibraryShcore = LoadLibrary(L"Shcore.dll");
		if (hLibraryShcore != nullptr)
		{
			CreateRandomAccessStreamOnFile = reinterpret_cast<CreateRandomAccessStreamOnFileType>(GetProcAddress(hLibraryShcore, "CreateRandomAccessStreamOnFile"));
		}

		hLibraryCombase = LoadLibrary(L"combase.dll");
		if (hLibraryCombase != nullptr)
		{
			WindowsCreateStringReference = reinterpret_cast<WindowsCreateStringReferenceType>(GetProcAddress(hLibraryCombase, "WindowsCreateStringReference"));
			RoGetActivationFactory = reinterpret_cast<RoGetActivationFactoryType>(GetProcAddress(hLibraryCombase, "RoGetActivationFactory"));
			RoActivateInstance = reinterpret_cast<RoActivateInstanceType>(GetProcAddress(hLibraryCombase, "RoActivateInstance"));
		}

		hLibraryD2D1 = LoadLibrary(L"D2D1.dll");
		if (hLibraryD2D1 != nullptr)
		{
			D2D1CreateFactory = reinterpret_cast<D2D1CreateFactoryType>(GetProcAddress(hLibraryD2D1, "D2D1CreateFactory"));
		}
	}

	void unload()
	{
		if (hLibraryShcore)
			FreeLibrary(hLibraryShcore);
		if (hLibraryCombase)
			FreeLibrary(hLibraryCombase);
		if (hLibraryD2D1)
			FreeLibrary(hLibraryD2D1);
	}
}

STDAPI CreateRandomAccessStreamOnFile(_In_ PCWSTR filePath, _In_ DWORD accessMode, _In_ REFIID riid, _COM_Outptr_ void **ppv)
{
	if (Win78Libraries::CreateRandomAccessStreamOnFile == nullptr)
		return E_FAIL;
	return Win78Libraries::CreateRandomAccessStreamOnFile(filePath, accessMode, riid, ppv);
}

STDAPI WindowsCreateStringReference(_In_reads_opt_(length + 1) PCWSTR sourceString, UINT32 length, _Out_ HSTRING_HEADER* hstringHeader, _Outptr_result_maybenull_ _Result_nullonfailure_ HSTRING* string)
{
	if (Win78Libraries::WindowsCreateStringReference == nullptr)
		return E_FAIL;
	return Win78Libraries::WindowsCreateStringReference(sourceString, length, hstringHeader, string);
}

HRESULT WINAPI D2D1CreateFactory(_In_ D2D1_FACTORY_TYPE factoryType, _In_ REFIID riid, _In_opt_ CONST D2D1_FACTORY_OPTIONS *pFactoryOptions, _Out_ void **ppIFactory)
{
	if (Win78Libraries::D2D1CreateFactory == nullptr)
		return E_FAIL;
	return Win78Libraries::D2D1CreateFactory(factoryType, riid, pFactoryOptions, ppIFactory);
}

#endif
