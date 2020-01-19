#pragma once

#ifdef _WIN64

#include <d2d1.h>
#include <roapi.h>

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
};

#endif