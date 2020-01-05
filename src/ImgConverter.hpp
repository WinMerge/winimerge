#pragma once

#ifdef _WIN64
#include <d2d1_3.h>
#include <wrl.h>
#pragma comment(lib, "d2d1")
#endif

#include "image.hpp"
#include <shlwapi.h>
#include <gdiplus.h>
#include <filesystem>

#pragma comment(lib, "shlwapi")
#pragma comment(lib, "gdiplus") 

struct ImageRenderer {
	virtual bool load(const wchar_t *filename) = 0;
	virtual bool isValid() const = 0;
	virtual void render(Image& img, float zoom) = 0;
};

#ifdef _WIN64
class SvgRenderer: public ImageRenderer {
public:
	virtual bool isValid() const
	{
		return m_pSvgDocument != nullptr;
	}

	virtual bool load(const wchar_t *filename)
	{
		if (!m_pD2DFactory)
		{
			if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_ID2D1Factory, &m_pD2DFactory)))
				return false;
		}

		if (!m_pDCRenderTarget)
		{
			const auto props = D2D1::RenderTargetProperties(
				D2D1_RENDER_TARGET_TYPE_DEFAULT,
				D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
				0,
				0,
				D2D1_RENDER_TARGET_USAGE_NONE,
				D2D1_FEATURE_LEVEL_DEFAULT
			);
			if (FAILED(m_pD2DFactory->CreateDCRenderTarget(&props, &m_pDCRenderTarget)))
				return false;
		}

		Microsoft::WRL::ComPtr<IStream> pStream;
		if (FAILED(::SHCreateStreamOnFileEx(filename, STGM_READ, 0, FALSE, nullptr, &pStream)))
			return false;

		Microsoft::WRL::ComPtr<ID2D1DeviceContext5> pDeviceContext5;
		if (FAILED(m_pDCRenderTarget->QueryInterface(IID_PPV_ARGS(&pDeviceContext5))))
			return false;
		if (FAILED(pDeviceContext5->CreateSvgDocument(pStream.Get(), { 1, 1 }, &m_pSvgDocument)))
			return false;

		calcSize();
		return true;
	}

	void render(Image& img, float zoom)
	{
		if (!isValid())
			return;
		RECT rc{ 0, 0, static_cast<long>(m_imageWidth * zoom), static_cast<long>(m_imageHeight * zoom) };
		HDC hDC = GetDC(nullptr);
		HDC hMemDC = CreateCompatibleDC(hDC);
		HBITMAP hBitmap = CreateCompatibleBitmap(hDC, rc.right - rc.left, rc.bottom - rc.top);
		HBITMAP hOldBitmap = static_cast<HBITMAP>(SelectObject(hMemDC, hBitmap));

		m_pDCRenderTarget->BindDC(hMemDC, &rc);
		m_pDCRenderTarget->BeginDraw();
		Microsoft::WRL::ComPtr<ID2D1DeviceContext5> pDeviceContext5;
		m_pDCRenderTarget->QueryInterface(IID_PPV_ARGS(&pDeviceContext5));
		m_pSvgDocument->SetViewportSize({ m_imageWidth, m_imageHeight });
		pDeviceContext5->SetTransform(D2D1::Matrix3x2F::Scale(zoom, zoom, { 0, 0 }));
		pDeviceContext5->DrawSvgDocument(m_pSvgDocument.Get());
		m_pDCRenderTarget->EndDraw();

		img.getFipImage()->copyFromBitmap(hBitmap);

		SelectObject(hMemDC, hOldBitmap);
		DeleteObject(hBitmap);
		DeleteDC(hMemDC);
		ReleaseDC(nullptr, hDC);
	}

private:
	void calcSize()
	{
		Microsoft::WRL::ComPtr<ID2D1SvgElement> pSvgElement;
		D2D1_SVG_VIEWBOX viewBox{};
		float width = 0.0f;
		float height = 0.0f;

		m_pSvgDocument->GetRoot(&pSvgElement);
		pSvgElement->GetAttributeValue(L"viewBox", D2D1_SVG_ATTRIBUTE_POD_TYPE_VIEWBOX, static_cast<void*>(&viewBox), sizeof(viewBox));
		pSvgElement->GetAttributeValue(L"width", &width);
		pSvgElement->GetAttributeValue(L"height", &height);

		if (viewBox.width != 0.0f && viewBox.height != 0.0f)
		{
			m_imageWidth = viewBox.width;
			m_imageHeight = viewBox.height;
		}
		if (height != 0.0f && width != 0.0f)
		{
			m_imageWidth = width;
			m_imageHeight = height;
		}
	}

private:
	Microsoft::WRL::ComPtr<ID2D1Factory> m_pD2DFactory;
	Microsoft::WRL::ComPtr<ID2D1DCRenderTarget>  m_pDCRenderTarget;
	Microsoft::WRL::ComPtr<ID2D1SvgDocument> m_pSvgDocument;
	float m_imageWidth = 0.0f;
	float m_imageHeight = 0.0f;
};


#endif

class GdiPlusRenderer: public ImageRenderer {
public:
	virtual bool isValid() const
	{
		return m_pMetafile != nullptr;
	}

	virtual bool load(const wchar_t *filename)
	{
		m_pMetafile.reset(new Gdiplus::Metafile(filename));
		if (!m_pMetafile)
			return false;
		
		std::wstring ext = std::filesystem::path(filename).extension().generic_wstring();
		if (_wcsicmp(ext.c_str(), L".wmf") == 0)
		{
			std::unique_ptr<Gdiplus::Bitmap> pBitmap(Gdiplus::Bitmap::FromFile(filename));
			m_imageWidth = pBitmap->GetWidth();
			m_imageHeight = pBitmap->GetHeight();
		}
		else
		{
			m_imageWidth = m_pMetafile->GetWidth();
			m_imageHeight = m_pMetafile->GetHeight();
		}
		return true;
	}

	void render(Image& img, float zoom)
	{
		HBITMAP hBitmap = nullptr;
		Gdiplus::Bitmap bitmap(static_cast<unsigned>(m_imageWidth * zoom), static_cast<unsigned>(m_imageHeight * zoom));
		std::unique_ptr<Gdiplus::Graphics> pGraphics(Gdiplus::Graphics::FromImage(&bitmap));
		pGraphics->ScaleTransform(zoom, zoom);
		pGraphics->DrawImage(m_pMetafile.get(), 0, 0);
		bitmap.GetHBITMAP({ 0, 0, 0, 0 }, &hBitmap);
		img.getFipImage()->copyFromBitmap(hBitmap);
		DeleteObject(hBitmap);
	}

private:
	std::unique_ptr<Gdiplus::Metafile> m_pMetafile;
	unsigned m_imageWidth = 0;
	unsigned m_imageHeight = 0;
};

class ImgConverter {
public:
	enum ImageType {
		NotSupported,
		SVG,
		EMF,
		WMF
	};

	static ImageType getImageType(const wchar_t *filename)
	{
		std::wstring ext = std::filesystem::path(filename).extension().generic_wstring();
		if (_wcsicmp(ext.c_str(), L".emf") == 0)
			return ImageType::EMF;
		else if (_wcsicmp(ext.c_str(), L".wmf") == 0)
			return ImageType::WMF;
#ifdef _WIN64
		else if (_wcsicmp(ext.c_str(), L".svg") == 0)
			return ImageType::SVG;
#endif
		return ImageType::NotSupported;
	}

	static bool isSupportedImage(const wchar_t *filename)
	{
		return getImageType(filename) != ImageType::NotSupported;
	}

	bool isValid() const
	{
		return (m_pRenderer != nullptr && m_pRenderer->isValid());
	}

	bool load(const wchar_t *filename)
	{
		switch (getImageType(filename))
		{
#ifdef _WIN64
		case ImageType::SVG:
			m_pRenderer.reset(new SvgRenderer());
			break;
#endif
		case ImageType::EMF:
		case ImageType::WMF:
			m_pRenderer.reset(new GdiPlusRenderer());
			break;
		default:
			return false;
		}
		return m_pRenderer->load(filename);
	}

	void close()
	{
		m_pRenderer.reset();
	}

	void render(Image& img, float zoom)
	{
		m_pRenderer->render(img, zoom);
	}

private:
	std::unique_ptr<ImageRenderer> m_pRenderer;
};

