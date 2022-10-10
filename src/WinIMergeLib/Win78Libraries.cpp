#ifdef _WIN64

#include "Win78Libraries.h"

namespace Win78Libraries
{
	CreateRandomAccessStreamOnFileType CreateRandomAccessStreamOnFile;
	WindowsCreateStringReferenceType WindowsCreateStringReference;
	WindowsDeleteStringType WindowsDeleteString;
	WindowsGetStringRawBufferType WindowsGetStringRawBuffer;
	RoGetActivationFactoryType RoGetActivationFactory;
	RoActivateInstanceType RoActivateInstance;
	D2D1CreateFactoryType D2D1CreateFactory;
	HMODULE hLibraryShcore;
	HMODULE hLibraryCombase;
	HMODULE hLibraryD2D1;
	CRITICAL_SECTION CriticalSection;

	void load()
	{
		if (HMODULE const h = hLibraryShcore ? nullptr : LoadLibrary(L"Shcore.dll"))
		{
			hLibraryShcore = h;
			CreateRandomAccessStreamOnFile = reinterpret_cast<CreateRandomAccessStreamOnFileType>(GetProcAddress(hLibraryShcore, "CreateRandomAccessStreamOnFile"));
		}

		if (HMODULE const h = hLibraryCombase ? nullptr : LoadLibrary(L"combase.dll"))
		{
			hLibraryCombase = h;
			WindowsCreateStringReference = reinterpret_cast<WindowsCreateStringReferenceType>(GetProcAddress(hLibraryCombase, "WindowsCreateStringReference"));
			WindowsDeleteString  = reinterpret_cast<WindowsDeleteStringType>(GetProcAddress(hLibraryCombase, "WindowsDeleteString"));
			WindowsGetStringRawBuffer = reinterpret_cast<WindowsGetStringRawBufferType>(GetProcAddress(hLibraryCombase, "WindowsGetStringRawBuffer"));
			RoGetActivationFactory = reinterpret_cast<RoGetActivationFactoryType>(GetProcAddress(hLibraryCombase, "RoGetActivationFactory"));
			RoActivateInstance = reinterpret_cast<RoActivateInstanceType>(GetProcAddress(hLibraryCombase, "RoActivateInstance"));
		}

		if (HMODULE const h = hLibraryD2D1 ? nullptr : LoadLibrary(L"D2D1.dll"))
		{
			hLibraryD2D1 = h;
			D2D1CreateFactory = reinterpret_cast<D2D1CreateFactoryType>(GetProcAddress(hLibraryD2D1, "D2D1CreateFactory"));
		}
	}

	void unload()
	{
		if (hLibraryShcore)
		{
			FreeLibrary(hLibraryShcore);
			hLibraryShcore = nullptr;
		}
		if (hLibraryCombase)
		{
			FreeLibrary(hLibraryCombase);
			hLibraryCombase = nullptr;
		}
		if (hLibraryD2D1)
		{
			FreeLibrary(hLibraryD2D1);
			hLibraryD2D1 = nullptr;
		}
	}

	HRESULT await(ABI::Windows::Foundation::IAsyncAction* pAsync)
	{
		Microsoft::WRL::Wrappers::Event event(CreateEventEx(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, WRITE_OWNER | EVENT_ALL_ACCESS));
		HRESULT hr = event.IsValid() ? S_OK : HRESULT_FROM_WIN32(GetLastError());
		if (FAILED(hr))
			return hr;

		HRESULT hrCallback = E_FAIL;
		hr = pAsync->put_Completed(
			Microsoft::WRL::Callback<ABI::Windows::Foundation::IAsyncActionCompletedHandler>(
				[&event, &hrCallback](_In_ ABI::Windows::Foundation::IAsyncAction* pAsync, AsyncStatus status)
		{
			hrCallback = (status == AsyncStatus::Completed) ? S_OK : E_FAIL;
			SetEvent(event.Get());
			return hrCallback;
		}).Get());
		if (FAILED(hr))
			return hr;

		WaitForSingleObjectEx(event.Get(), INFINITE, FALSE);
		return hrCallback;
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

STDAPI WindowsDeleteString(_In_opt_ HSTRING string)
{
	if (Win78Libraries::WindowsDeleteString == nullptr)
		return E_FAIL;
	return Win78Libraries::WindowsDeleteString(string);
}

STDAPI_(PCWSTR) WindowsGetStringRawBuffer(_In_opt_ HSTRING string, _Out_opt_ UINT32* length)
{
	if (Win78Libraries::WindowsGetStringRawBuffer == nullptr)
		return nullptr;
	return Win78Libraries::WindowsGetStringRawBuffer(string, length);
}

HRESULT WINAPI D2D1CreateFactory(_In_ D2D1_FACTORY_TYPE factoryType, _In_ REFIID riid, _In_opt_ CONST D2D1_FACTORY_OPTIONS *pFactoryOptions, _Out_ void **ppIFactory)
{
	if (Win78Libraries::D2D1CreateFactory == nullptr)
		return E_FAIL;
	return Win78Libraries::D2D1CreateFactory(factoryType, riid, pFactoryOptions, ppIFactory);
}

#endif
