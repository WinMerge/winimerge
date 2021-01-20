#pragma once

#include <optional>
#include <vector>

namespace ocr
{
struct Rect
{
	float x;
	float y;
	float width;
	float height;
};

struct Word
{
	std::wstring text;
	Rect rect;
};

struct Line
{
	std::wstring text;
	std::vector<Word> words;
};

struct Result
{
	std::wstring text;
	std::optional<double> textAngle;
	std::vector<Line> lines;
};

}

#ifdef _WIN64
#include <Windows.Storage.h>
#include <Windows.Storage.Streams.h>
#include <Windows.Graphics.Imaging.h>
#include <Windows.Media.Ocr.h>
#include <Windows.Foundation.Collections.h>
#include <wrl.h>
#include "Win78Libraries.h"

namespace ocr
{

using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Foundation::IReference;
using ABI::Windows::Foundation::Collections::IVectorView;
using ABI::Windows::Graphics::Imaging::IBitmapDecoder;
using ABI::Windows::Graphics::Imaging::IBitmapDecoderStatics;
using ABI::Windows::Graphics::Imaging::ISoftwareBitmap;
using ABI::Windows::Graphics::Imaging::ISoftwareBitmapStatics;
using ABI::Windows::Graphics::Imaging::IBitmapFrameWithSoftwareBitmap;
using ABI::Windows::Graphics::Imaging::BitmapDecoder;
using ABI::Windows::Graphics::Imaging::SoftwareBitmap;
using ABI::Windows::Media::Ocr::IOcrEngine;
using ABI::Windows::Media::Ocr::IOcrEngineStatics;
using ABI::Windows::Media::Ocr::IOcrLine;
using ABI::Windows::Media::Ocr::IOcrWord;
using ABI::Windows::Media::Ocr::IOcrResult;
using ABI::Windows::Media::Ocr::OcrLine;
using ABI::Windows::Media::Ocr::OcrWord;
using ABI::Windows::Media::Ocr::OcrResult;
using ABI::Windows::Storage::Streams::IRandomAccessStream;
using ABI::Windows::Storage::FileAccessMode_Read;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Wrappers::Event;
using Microsoft::WRL::Wrappers::HStringReference;
using Microsoft::WRL::Wrappers::HString;
using Thread = Microsoft::WRL::Wrappers::HandleT<Microsoft::WRL::Wrappers::HandleTraits::HANDLENullTraits>;

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
			Win78Libraries::load();
			if (Win78Libraries::RoGetActivationFactory == nullptr)
				return false;
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

	bool extractText(Result& result)
	{
		if (!isValid())
			return false;

		Event recognizeCompleted(CreateEventEx(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, WRITE_OWNER | EVENT_ALL_ACCESS));

		OcrThreadParams params{};
		params.hEvent = recognizeCompleted.Get();
		params.type = 1;

		PostThreadMessage(m_dwThreadId, WM_USER, 0, reinterpret_cast<LPARAM>(&params));

		WaitForSingleObject(params.hEvent, INFINITE);

		result = std::move(params.ocrResult);
		return params.result;
	}

private:

	struct OcrThreadParams
	{
		const wchar_t* filename;
		int type;
		HANDLE hEvent;
		bool result;
		Result ocrResult;
	};

	static bool LoadImage(const wchar_t* filename, ISoftwareBitmap** ppBitmap)
	{
		ComPtr<IRandomAccessStream> s;
		HRESULT hr = CreateRandomAccessStreamOnFile(filename, static_cast<DWORD>(FileAccessMode_Read),
			IID_PPV_ARGS(&s));
		if (FAILED(hr))
			return false;

		ComPtr<IBitmapDecoderStatics> pBitmapDecoderStatics;
		hr = Win78Libraries::RoGetActivationFactory(
			HStringReference(RuntimeClass_Windows_Graphics_Imaging_BitmapDecoder).Get(),
			IID_PPV_ARGS(&pBitmapDecoderStatics));
		if (FAILED(hr))
			return false;

		ComPtr<IAsyncOperation<BitmapDecoder*>> pAsync;
		hr = pBitmapDecoderStatics->CreateAsync(s.Get(), &pAsync);
		if (FAILED(hr))
			return false;

		ComPtr<IBitmapDecoder> pBitmapDecoder;
		hr = Win78Libraries::await(pAsync.Get(), pBitmapDecoder.GetAddressOf());
		if (FAILED(hr))
			return false;

		ComPtr<IBitmapFrameWithSoftwareBitmap> pBitmapFrameWithSoftwareBitmap;
		hr = pBitmapDecoder->QueryInterface<IBitmapFrameWithSoftwareBitmap>(&pBitmapFrameWithSoftwareBitmap);
		if (FAILED(hr))
			return false;

		ComPtr<IAsyncOperation<SoftwareBitmap*>> pAsync2;
		hr = pBitmapFrameWithSoftwareBitmap->GetSoftwareBitmapAsync(&pAsync2);
		if (FAILED(hr))
			return false;

		return SUCCEEDED(Win78Libraries::await(pAsync2.Get(), ppBitmap));
	}

	static IOcrEngine* CreateOcrEngine()
	{
		ComPtr<IOcrEngineStatics> pOcrEngineStatics;
		HRESULT hr = Win78Libraries::RoGetActivationFactory(
			HStringReference(RuntimeClass_Windows_Media_Ocr_OcrEngine).Get(),
			IID_PPV_ARGS(&pOcrEngineStatics));
		if (FAILED(hr))
			return nullptr;

		IOcrEngine* pOcrEngine = nullptr;
		hr = pOcrEngineStatics->TryCreateFromUserProfileLanguages(&pOcrEngine);
		if (FAILED(hr))
			return nullptr;

		return pOcrEngine;
	}

	static std::wstring HStringToWstring(const HString& htext)
	{
		unsigned int length = 0;
		const wchar_t* ptr = htext.GetRawBuffer(&length);
		if (!ptr)
			return L"";
		return std::wstring(ptr, length);
	}

	static bool Recognize(IOcrEngine* pOcrEngine, ISoftwareBitmap* pBitmap, Result& result)
	{
		result.textAngle.reset();
		result.lines.clear();

		if (!pOcrEngine || !pBitmap)
			return false;

		ComPtr<IAsyncOperation<OcrResult*>> pAsync;
		HRESULT hr = pOcrEngine->RecognizeAsync(pBitmap, &pAsync);
		if (FAILED(hr))
			return false;

		ComPtr<IOcrResult> pOcrResult;
		hr = Win78Libraries::await(pAsync.Get(), pOcrResult.GetAddressOf());
		if (FAILED(hr))
			return false;

		HString htext;
		ComPtr<IReference<double>> pTextAngle;
		pOcrResult->get_TextAngle(&pTextAngle);
		if (pTextAngle)
		{
			double angle;
			pTextAngle->get_Value(&angle);
			result.textAngle = angle;
		}

		ComPtr<IVectorView<OcrLine*>> pOcrLines;
		hr = pOcrResult->get_Lines(&pOcrLines);
		if (FAILED(hr))
			return false;

		uint32_t nlines;
		hr = pOcrLines->get_Size(&nlines);
		if (FAILED(hr))
			return false;

		for (uint32_t i = 0; i < nlines; ++i)
		{
			Line line;
			ComPtr<IOcrLine> pOcrLine;
			hr = pOcrLines->GetAt(i, &pOcrLine);
			if (FAILED(hr))
				break;

			ComPtr<IVectorView<OcrWord*>> pOcrWords;
			hr = pOcrLine->get_Words(&pOcrWords);
			if (FAILED(hr))
				return false;

			uint32_t nwords;
			hr = pOcrWords->get_Size(&nwords);
			if (FAILED(hr))
				return false;

			for (uint32_t j = 0; j < nwords; ++j)
			{
				ComPtr<IOcrWord> pOcrWord;
				hr = pOcrWords->GetAt(j, &pOcrWord);
				if (FAILED(hr))
					break;

				ABI::Windows::Foundation::Rect rect;
				hr = pOcrWord->get_BoundingRect(&rect);

				hr = pOcrWord->get_Text(htext.GetAddressOf());
				if (FAILED(hr))
					return false;

				std::wstring text = HStringToWstring(htext);
				Word word{ text, Rect{rect.X, rect.Y, rect.Width, rect.Height}};

				if (line.words.size() > 0)
				{
					const Word& prevword = line.words[line.words.size() - 1];
					if (isascii(prevword.text.back()) || 
						word.rect.x - (prevword.rect.x + prevword.rect.width) > word.rect.height / 2)
					{
						line.text.append(L" ");
					}
				}

				line.words.emplace_back(word);

				line.text.append(text);
			}

			result.lines.emplace_back(line);

			result.text.append(line.text);
			result.text.append(L"\n");
		}

		return true;
	}

	static DWORD WINAPI OcrWorkerThread(LPVOID lpParam)
	{
		HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
		{
			ComPtr<ISoftwareBitmap> pBitmap;
			ComPtr<IOcrEngine> pOcrEngine(CreateOcrEngine());
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
					pParam->result = Recognize(pOcrEngine.Get(), pBitmap.Get(), pParam->ocrResult);
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
	Thread m_thread;
	DWORD m_dwThreadId = 0;
};

}

#else

namespace ocr
{

class COcr
{
public:
	bool isValid() const { return false; }
	bool load(const wchar_t* filename) { return false; }
	bool extractText(Result& result)
	{
		const std::wstring msg = L"This function is not supported on 32bit version";
		result.text = msg;
		Line line;
		line.text = msg;
		Word word;
		word.text = msg;
		line.words.emplace_back(word);
		result.lines.emplace_back(line);
		return false;
	}
};

}

#endif
