#pragma once

#ifdef _WIN64

#include <d2d1.h>
#include <roapi.h>
#include <windows.foundation.h>
#include <wrl.h>

namespace Win78Libraries
{
	using CreateRandomAccessStreamOnFileType = HRESULT(__stdcall*)(_In_ PCWSTR filePath, _In_ DWORD accessMode, _In_ REFIID riid, _COM_Outptr_ void** ppv);
	using WindowsCreateStringReferenceType = HRESULT(__stdcall*)(_In_reads_opt_(length + 1) PCWSTR sourceString, UINT32 length, _Out_ HSTRING_HEADER * hstringHeader, _Outptr_result_maybenull_ _Result_nullonfailure_ HSTRING * string);
	using RoGetActivationFactoryType = HRESULT(__stdcall*)(HSTRING activatableClassId, REFIID iid, void** factory);
	using RoActivateInstanceType = HRESULT(__stdcall*)(HSTRING activatableClassId, IInspectable * *instance);
	using D2D1CreateFactoryType = HRESULT(__stdcall*)(_In_ D2D1_FACTORY_TYPE factoryType, _In_ REFIID riid, _In_opt_ CONST D2D1_FACTORY_OPTIONS * pFactoryOptions, _Out_ void** ppIFactory);
	extern CreateRandomAccessStreamOnFileType CreateRandomAccessStreamOnFile;
	extern WindowsCreateStringReferenceType WindowsCreateStringReference;
	extern RoGetActivationFactoryType RoGetActivationFactory;
	extern RoActivateInstanceType RoActivateInstance;
	extern D2D1CreateFactoryType D2D1CreateFactory;

	void load();
	void unload();

	template <class T, class R>
	HRESULT await(ABI::Windows::Foundation::IAsyncOperation<T*> *pAsync, R **ppResult)
	{
		Microsoft::WRL::Wrappers::Event event(CreateEventEx(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, WRITE_OWNER | EVENT_ALL_ACCESS));
		HRESULT hr = event.IsValid() ? S_OK : HRESULT_FROM_WIN32(GetLastError());
		if (FAILED(hr))
			return false;

		HRESULT hrCallback = E_FAIL;
		hr = pAsync->put_Completed(
			Microsoft::WRL::Callback<ABI::Windows::Foundation::IAsyncOperationCompletedHandler<T*>>(
				[&event, &hrCallback, ppResult](_In_ ABI::Windows::Foundation::IAsyncOperation<T*>* pAsync, AsyncStatus status)
		{
			hrCallback = (status == AsyncStatus::Completed) ? pAsync->GetResults(ppResult) : E_FAIL;
			SetEvent(event.Get());
			return hrCallback;
		}).Get());
		if (FAILED(hr))
			return false;

		WaitForSingleObjectEx(event.Get(), INFINITE, FALSE);
		return SUCCEEDED(hrCallback);
	}

	HRESULT await(ABI::Windows::Foundation::IAsyncAction* pAsync);
};

#endif