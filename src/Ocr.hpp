#pragma once

#ifdef _WIN64
#include <Windows.Storage.h>
#include <Windows.Storage.Streams.h>
#include <Windows.Graphics.Imaging.h>
#include <Windows.Media.Ocr.h>
#include <Windows.Foundation.Collections.h>
#include <wrl.h>
#include "Win78Libraries.h"

class COcr
{
public:
	~COcr()
	{
		if (m_thread.Get())
		{
			PostThreadMessage(m_dwThreadId, WM_QUIT, 0, 0);
		}
	}

	bool isValid() const
	{
		return m_thread.IsValid();
	}

	bool load(const wchar_t* filename)
	{
		Microsoft::WRL::Wrappers::Event loadCompleted(CreateEventEx(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, WRITE_OWNER | EVENT_ALL_ACCESS));

		OcrThreadParams params{};
		params.filename = filename;
		params.hEvent = loadCompleted.Get();
		params.type = 0;
		params.result = false;

		if (!m_thread.IsValid())
		{
			m_thread.Attach(CreateThread(nullptr, 0, OcrWorkerThread, &params, 0, &m_dwThreadId));
			if (!m_thread.IsValid())
				return false;
		}
		else
		{
			PostThreadMessage(m_dwThreadId, WM_USER, 0, reinterpret_cast<LPARAM>(&params));
		}

		WaitForSingleObject(params.hEvent, INFINITE);

		return params.result;
	}

	bool extractText(std::wstring& text)
	{
		if (!isValid())
			return false;

		Microsoft::WRL::Wrappers::Event recognizeCompleted(CreateEventEx(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, WRITE_OWNER | EVENT_ALL_ACCESS));

		OcrThreadParams params{};
		params.hEvent = recognizeCompleted.Get();
		params.type = 1;
			
		PostThreadMessage(m_dwThreadId, WM_USER, 0, reinterpret_cast<LPARAM>(&params));

		WaitForSingleObject(params.hEvent, INFINITE);

		text = params.text;
		return params.result;
	}

private:

	struct OcrThreadParams
	{
		const wchar_t *filename;
		int type;
		HANDLE hEvent;
		bool result;
		std::wstring text;
	};

	static bool LoadImage(const wchar_t *filename, ABI::Windows::Graphics::Imaging::ISoftwareBitmap **ppBitmap)
	{
		Microsoft::WRL::ComPtr<ABI::Windows::Storage::Streams::IRandomAccessStream> s;
		HRESULT hr = CreateRandomAccessStreamOnFile(filename, static_cast<DWORD>(ABI::Windows::Storage::FileAccessMode_Read),
			IID_PPV_ARGS(&s));
		if (FAILED(hr))
			return false;

		Microsoft::WRL::ComPtr<ABI::Windows::Graphics::Imaging::IBitmapDecoderStatics> pBitmapDecoderStatics;
		hr = Win78Libraries::RoGetActivationFactory(
			Microsoft::WRL::Wrappers::HStringReference(RuntimeClass_Windows_Graphics_Imaging_BitmapDecoder).Get(),
			IID_PPV_ARGS(&pBitmapDecoderStatics));
		if (FAILED(hr))
			return false;

		Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IAsyncOperation<ABI::Windows::Graphics::Imaging::BitmapDecoder*>> pAsync;
		hr = pBitmapDecoderStatics->CreateAsync(s.Get(), &pAsync);
		if (FAILED(hr))
			return false;

		Microsoft::WRL::ComPtr<ABI::Windows::Graphics::Imaging::IBitmapDecoder> pBitmapDecoder;
		hr = Win78Libraries::await(pAsync.Get(), pBitmapDecoder.GetAddressOf());
		if (FAILED(hr))
			return false;

		Microsoft::WRL::ComPtr<ABI::Windows::Graphics::Imaging::IBitmapFrameWithSoftwareBitmap> pBitmapFrameWithSoftwareBitmap;
		hr = pBitmapDecoder->QueryInterface<ABI::Windows::Graphics::Imaging::IBitmapFrameWithSoftwareBitmap>(&pBitmapFrameWithSoftwareBitmap);
		if (FAILED(hr))
			return false;

		Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IAsyncOperation<ABI::Windows::Graphics::Imaging::SoftwareBitmap*>> pAsync2;
		hr = pBitmapFrameWithSoftwareBitmap->GetSoftwareBitmapAsync(&pAsync2);
		if (FAILED(hr))
			return false;

		return SUCCEEDED(Win78Libraries::await(pAsync2.Get(), ppBitmap));
	}

	static ABI::Windows::Media::Ocr::IOcrEngine* CreateOcrEngine()
	{
		Microsoft::WRL::ComPtr<ABI::Windows::Media::Ocr::IOcrEngineStatics> pOcrEngineStatics;
		HRESULT hr = Win78Libraries::RoGetActivationFactory(
			Microsoft::WRL::Wrappers::HStringReference(RuntimeClass_Windows_Media_Ocr_OcrEngine).Get(),
			IID_PPV_ARGS(&pOcrEngineStatics));
		if (FAILED(hr))
			return nullptr;

		ABI::Windows::Media::Ocr::IOcrEngine *pOcrEngine = nullptr;
		hr = pOcrEngineStatics->TryCreateFromUserProfileLanguages(&pOcrEngine);
		if (FAILED(hr))
			return nullptr;

		return pOcrEngine;
	}

	static bool Recognize(ABI::Windows::Media::Ocr::IOcrEngine *pOcrEngine,
		ABI::Windows::Graphics::Imaging::ISoftwareBitmap *pBitmap,
		std::wstring& text)
	{
		text.clear();

		if (!pOcrEngine || !pBitmap)
			return false;

		Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IAsyncOperation<ABI::Windows::Media::Ocr::OcrResult*>> pAsync;
		HRESULT hr = pOcrEngine->RecognizeAsync(pBitmap, &pAsync);
		if (FAILED(hr))
			return false;

		Microsoft::WRL::ComPtr<ABI::Windows::Media::Ocr::IOcrResult> pOcrResult;
		hr = Win78Libraries::await(pAsync.Get(), pOcrResult.GetAddressOf());
		if (FAILED(hr))
			return false;

		Microsoft::WRL::Wrappers::HString htext;
		hr = pOcrResult->get_Text(htext.GetAddressOf());
		if (FAILED(hr))
			return false;

		unsigned int length = 0;
		const wchar_t *ptr = htext.GetRawBuffer(&length);
		if (!ptr)
			return false;

		text = std::wstring(ptr, length);

		return true;
	}

	static DWORD WINAPI OcrWorkerThread(LPVOID lpParam)
	{
		if (Win78Libraries::RoGetActivationFactory == nullptr)
			Win78Libraries::load();

		HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
		{
			Microsoft::WRL::ComPtr<ABI::Windows::Graphics::Imaging::ISoftwareBitmap> pBitmap;
			Microsoft::WRL::ComPtr<ABI::Windows::Media::Ocr::IOcrEngine> pOcrEngine(CreateOcrEngine());
			OcrThreadParams* pParam = reinterpret_cast<OcrThreadParams*>(lpParam);
			for (;;)
			{
				if (pParam->type == 0)
				{
					pBitmap.Reset();
					pParam->result = LoadImage(pParam->filename, &pBitmap);
				}
				else
				{
					pParam->result = Recognize(pOcrEngine.Get(), pBitmap.Get(), pParam->text);
				}
				SetEvent(pParam->hEvent);

				MSG msg;
				BOOL bRet = GetMessage(&msg, nullptr, 0, 0);
				if (bRet == 0 || bRet == -1)
					break;
				pParam = reinterpret_cast<OcrThreadParams*>(msg.lParam);
			}
		}
		CoUninitialize();
		return true;
	}

private:
	using Thread = Microsoft::WRL::Wrappers::HandleT<Microsoft::WRL::Wrappers::HandleTraits::HANDLENullTraits>;
	Thread m_thread;
	DWORD m_dwThreadId = 0;
};

#else

class COcr
{
public:
	bool isValid() const { return false; }
	bool load(const wchar_t* filename) { return false; }
	bool extractText(std::wstring& text) { return false; }
};

#endif
