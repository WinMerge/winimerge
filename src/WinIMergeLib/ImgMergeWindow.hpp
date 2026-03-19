/////////////////////////////////////////////////////////////////////////////
//    License (GPLv2+):
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful, but
//    WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//    General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
/////////////////////////////////////////////////////////////////////////////

#pragma once
#pragma warning(disable: 4819)

#include <Windows.h>
#include <cstring>
#include <chrono>
#include "FreeImagePlus.h"
#include "ImgWindow.hpp"
#include "ImgMergeBuffer.hpp"
#include "Ocr.hpp"
#include "WinIMergeLib.h"


namespace
{
	RGBQUAD COLORREFtoRGBQUAD(COLORREF c)
	{
		RGBQUAD rgb;
		rgb.rgbRed   = GetRValue(c);
		rgb.rgbGreen = GetGValue(c);
		rgb.rgbBlue  = GetBValue(c);
		rgb.rgbReserved = (c >> 24);
		return rgb;
	}

	COLORREF RGBQUADtoCOLORREF(RGBQUAD c)
	{
		return RGB(c.rgbRed, c.rgbGreen, c.rgbBlue) | (c.rgbReserved << 24);
	}
}

class CImgMergeWindow : public IImgMergeWindow
{
	static constexpr int TIMER_INTERVAL = 25;

	struct EventListenerInfo 
	{
		EventListenerInfo(EventListenerFunc func, void *userdata) : func(func), userdata(userdata) {}
		EventListenerFunc func;
		void *userdata;
	};

public:
	CImgMergeWindow() : 
		  m_nImages(0)
		, m_hWnd(NULL)
		, m_hInstance(NULL)
		, m_nDraggingSplitter(-1)
		, m_bHorizontalSplit(false)
		, m_oldSplitPosX(-4)
		, m_oldSplitPosY(-4)
		, m_bDragging(false)
		, m_ptOrg{ 0, 0 }
		, m_ptPrev{ 0, 0 }
		, m_draggingMode(DRAGGING_MODE::MOVE)
		, m_draggingModeCurrent(DRAGGING_MODE::MOVE)
		, m_gdiplusToken(0)
		, m_timerPrev()
		, m_timerNext()
	{
		for (int i = 0; i < 3; ++i)
			m_ChildWndProc[i] = NULL;

		Gdiplus::GdiplusStartupInput gdiplusStartupInput;
		Gdiplus::GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, nullptr);
	}

	~CImgMergeWindow()
	{
		Gdiplus::GdiplusShutdown(m_gdiplusToken);
	}

	bool Create(HINSTANCE hInstance, HWND hWndParent, int nID, const RECT &rc)
	{
		m_hInstance = hInstance;
		MyRegisterClass(hInstance);
		m_hWnd = CreateWindowExW(0, L"WinImgMergeWindowClass", NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
			rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, hWndParent, reinterpret_cast<HMENU>((intptr_t)nID), hInstance, this);
		return m_hWnd ? true : false;
	}

	bool Destroy()
	{
		BOOL bSucceeded = true;
		if (m_hWnd)
			bSucceeded = DestroyWindow(m_hWnd);
		m_hWnd = NULL;
		return !!bSucceeded;
	}

	void AddEventListener(EventListenerFunc func, void *userdata)
	{
		m_listener.push_back(EventListenerInfo(func, userdata));
	}

	const wchar_t *GetFileName(int pane) override
	{
		return m_buffer.GetFileName(pane);
	}

	int GetPaneCount() const override
	{
		return m_nImages;
	}

	RECT GetPaneWindowRect(int pane) const override
	{
		if (pane < 0 || pane >= m_nImages || !m_hWnd)
		{
			RECT rc = {-1, -1, -1, -1};
			return rc;
		}
		return m_imgWindow[pane].GetWindowRect();
	}

	RECT GetWindowRect() const override
	{
		if (!m_hWnd)
			return RECT{0};
		RECT rc, rcParent;
		HWND hwndParent = GetParent(m_hWnd);
		::GetWindowRect(hwndParent, &rcParent);
		::GetWindowRect(m_hWnd, &rc);
		rc.left   -= rcParent.left;
		rc.top    -= rcParent.top;
		rc.right  -= rcParent.left;
		rc.bottom -= rcParent.top;
		return rc;
	}

	bool SetWindowRect(const RECT& rc) override
	{
		if (m_hWnd)
			MoveWindow(m_hWnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
		return true;
	}

	POINT GetCursorPos(int pane) const override
	{
		POINT pt = {-1, -1};
		if (pane < 0 || pane > m_nImages || !m_hWnd)
			return pt;
		return m_imgWindow[pane].GetCursorPos();
	}

	bool ConvertToRealPos(int pane, const POINT& pt, POINT& ptReal) const override
	{
		int rx, ry;
		bool result = m_buffer.ConvertToRealPos(pane, pt.x, pt.y, rx, ry);
		ptReal = { rx, ry };
		return result;
	}

	RGBQUAD GetPixelColor(int pane, int x, int y) const override
	{
		return m_buffer.GetPixelColor(pane, x, y);
	}

	double GetColorDistance(int pane1, int pane2, int x, int y) const override
	{
		return m_buffer.GetColorDistance(pane1, pane2, x, y);
	}

	int GetActivePane() const override
	{
		if (!m_hWnd)
			return -1;
		for (int i = 0; i < m_nImages; ++i)
			if (m_imgWindow[i].IsFocused())
				return i;
		return -1;
	}

	void SetActivePane(int pane) override
	{
		if (pane < 0 || pane >= m_nImages || !m_hWnd)
			return;
		m_imgWindow[pane].SetFocus();
	}

	bool GetReadOnly(int pane) const override
	{
		return m_buffer.GetReadOnly(pane);
	}

	void SetReadOnly(int pane, bool readOnly) override
	{
		m_buffer.SetReadOnly(pane, readOnly);
	}

	bool GetHorizontalSplit() const override
	{
		return m_bHorizontalSplit;
	}

	void SetHorizontalSplit(bool horizontalSplit) override
	{
		if (!m_hWnd)
			return;
		m_bHorizontalSplit = horizontalSplit;
		std::vector<RECT> rects = CalcChildImgWindowRect(m_hWnd, m_nImages, m_bHorizontalSplit);
		for (int i = 0; i < m_nImages; ++i)
		{
			if (i < m_nImages - 1)
				m_imgWindow[i].SetScrollBar(m_bHorizontalSplit ? SB_VERT : SB_HORZ);
			m_imgWindow[i].SetWindowRect(rects[i]);
		}
	}

	COLORREF GetDiffColor() const override
	{
		return RGBQUADtoCOLORREF(m_buffer.GetDiffColor());
	}

	void SetDiffColor(COLORREF clrDiffColor) override
	{
		m_buffer.SetDiffColor(COLORREFtoRGBQUAD(clrDiffColor));
		Invalidate();
	}

	COLORREF GetDiffDeletedColor() const override
	{
		return RGBQUADtoCOLORREF(m_buffer.GetDiffDeletedColor());
	}

	void SetDiffDeletedColor(COLORREF clrDiffDeletedColor) override
	{
		m_buffer.SetDiffDeletedColor(COLORREFtoRGBQUAD(clrDiffDeletedColor));
		Invalidate();
	}

	COLORREF GetSelDiffColor() const override
	{
		return RGBQUADtoCOLORREF(m_buffer.GetSelDiffColor());
	}

	void SetSelDiffColor(COLORREF clrSelDiffColor) override
	{
		m_buffer.SetSelDiffColor(COLORREFtoRGBQUAD(clrSelDiffColor));
		Invalidate();
	}

	COLORREF GetSelDiffDeletedColor() const override
	{
		return RGBQUADtoCOLORREF(m_buffer.GetSelDiffDeletedColor());
	}

	void SetSelDiffDeletedColor(COLORREF clrSelDiffDeletedColor) override
	{
		m_buffer.SetSelDiffColor(COLORREFtoRGBQUAD(clrSelDiffDeletedColor));
		Invalidate();
	}

	double GetDiffColorAlpha() const override
	{
		return m_buffer.GetDiffColorAlpha();
	}

	void SetDiffColorAlpha(double diffColorAlpha) override
	{
		m_buffer.SetDiffColorAlpha(diffColorAlpha);
		Invalidate();
	}

	RGBQUAD GetBackColor() const override
	{
		return m_hWnd ? m_imgWindow[0].GetBackColor() : RGBQUAD{0};
	}

	void SetBackColor(RGBQUAD backColor) override
	{
		if (m_hWnd)
		{
			for (int i = 0; i < 3; ++i)
				m_imgWindow[i].SetBackColor(backColor);
		}
	}

	bool GetUseBackColor() const override
	{
		return m_hWnd ? m_imgWindow[0].GetUseBackColor() : false;
	}

	void SetUseBackColor(bool useBackColor) override
	{
		if (!m_hWnd)
			return;
		for (int i = 0; i < 3; ++i)
			m_imgWindow[i].SetUseBackColor(useBackColor);
	}

	double GetZoom() const override
	{
		return m_hWnd ? m_imgWindow[0].GetZoom() : 1.0;
	}

	void SetZoom(double zoom) override
	{
		if (!m_hWnd)
			return;
		for (int i = 0; i < 3; ++i)
			m_imgWindow[i].SetZoom(zoom);

		Event evt;
		evt.eventType = REFRESH;
		notify(evt);
	}

	int  GetCurrentPage(int pane) const override
	{
		return m_buffer.GetCurrentPage(pane);
	}

	void SetCurrentPage(int pane, int page) override
	{
		m_buffer.SetCurrentPage(pane, page);
		Invalidate();
	}

	void SetCurrentPageAll(int page) override
	{
		m_buffer.SetCurrentPageAll(page);
		Invalidate();
	}

	int  GetCurrentMaxPage() const override
	{
		return m_buffer.GetCurrentMaxPage();
	}

	int  GetPageCount(int pane) const override
	{
		return m_buffer.GetPageCount(pane);
	}

	int  GetMaxPageCount() const override
	{
		return m_buffer.GetMaxPageCount();
	}

	double GetColorDistanceThreshold() const override
	{
		return m_buffer.GetColorDistanceThreshold();
	}

	void SetColorDistanceThreshold(double threshold) override
	{
		m_buffer.SetColorDistanceThreshold(threshold);
		Invalidate();
	}

	int  GetDiffBlockSize() const override
	{
		return m_buffer.GetDiffBlockSize();
	}
	
	void SetDiffBlockSize(int blockSize) override
	{
		m_buffer.SetDiffBlockSize(blockSize);
		Invalidate();
	}

	INSERTION_DELETION_DETECTION_MODE GetInsertionDeletionDetectionMode() const override
	{
		return static_cast<INSERTION_DELETION_DETECTION_MODE>(m_buffer.GetInsertionDeletionDetectionMode());
	}

	void SetInsertionDeletionDetectionMode(INSERTION_DELETION_DETECTION_MODE insertionDeletinoMode) override
	{
		m_buffer.SetInsertionDeletionDetectionMode(static_cast<CImgMergeBuffer::INSERTION_DELETION_DETECTION_MODE>(insertionDeletinoMode));
		Invalidate();
	}

	OVERLAY_MODE GetOverlayMode() const override
	{
		return static_cast<OVERLAY_MODE>(m_buffer.GetOverlayMode());
	}

	void SetOverlayMode(OVERLAY_MODE overlayMode) override
	{
		m_buffer.SetOverlayMode(static_cast<CImgMergeBuffer::OVERLAY_MODE>(overlayMode));
		Invalidate();
		if (overlayMode == OVERLAY_ALPHABLEND_ANIM || m_buffer.GetBlinkDifferences())
			SetTimer(m_hWnd, 2, TIMER_INTERVAL, NULL);
		else
			KillTimer(m_hWnd, 2);
		m_timerNext = {};
	}

	double GetOverlayAlpha() const override
	{
		return m_buffer.GetOverlayAlpha();
	}

	void SetOverlayAlpha(double overlayAlpha) override
	{
		m_buffer.SetOverlayAlpha(overlayAlpha);
		Invalidate();
	}

	bool GetShowDifferences() const override
	{
		return m_buffer.GetShowDifferences();
	}

	void SetShowDifferences(bool visible) override
	{
		m_buffer.SetShowDifferences(visible);
		Invalidate();
	}

	bool GetBlinkDifferences() const override
	{
		return m_buffer.GetBlinkDifferences();
	}

	void SetBlinkDifferences(bool blink) override
	{
		m_buffer.SetBlinkDifferences(blink);
		Invalidate();
		if (blink || m_buffer.GetOverlayMode() == OVERLAY_ALPHABLEND_ANIM)
			SetTimer(m_hWnd, 2, TIMER_INTERVAL, NULL);
		else
			KillTimer(m_hWnd, 2);
		m_timerNext = {};
	}

	float GetVectorImageZoomRatio() const override
	{
		return m_buffer.GetVectorImageZoomRatio();
	}

	void SetVectorImageZoomRatio(float zoom) override
	{
		m_buffer.SetVectorImageZoomRatio(zoom);
		Invalidate();
	}

	float GetRotation(int pane) const override
	{
		return m_buffer.GetRotation(pane);
	}

	void SetRotation(int pane, float angle) override
	{
		m_buffer.SetRotation(pane, angle);
		Invalidate();
	}

	bool GetHorizontalFlip(int pane) const override
	{
		return m_buffer.GetHorizontalFlip(pane);
	}

	void SetHorizontalFlip(int pane, bool flip) override
	{
		m_buffer.SetHorizontalFlip(pane, flip);
		Invalidate();
	}

	bool GetVerticalFlip(int pane) const override
	{
		return m_buffer.GetVerticalFlip(pane);
	}

	void SetVerticalFlip(int pane, bool flip) override
	{
		m_buffer.SetVerticalFlip(pane, flip);
		Invalidate();
	}

	DIFF_ALGORITHM GetDiffAlgorithm() const
	{
		return static_cast<DIFF_ALGORITHM>(m_buffer.GetDiffAlgorithm());
	}

	void SetDiffAlgorithm(DIFF_ALGORITHM diffAlgorithm)
	{
		m_buffer.SetDiffAlgorithm(static_cast<CImgMergeBuffer::DIFF_ALGORITHM>(diffAlgorithm));
		Invalidate();
	}

	int  GetDiffCount() const override
	{
		return m_buffer.GetDiffCount();
	}

	int  GetConflictCount() const override
	{
		return m_buffer.GetConflictCount();
	}

	int  GetCurrentDiffIndex() const override
	{
		return m_buffer.GetCurrentDiffIndex();
	}

	bool FirstDiff() override
	{
		bool result = m_buffer.FirstDiff();
		if (result)
			ScrollToDiff(m_buffer.GetCurrentDiffIndex());
		return result;
	}

	bool LastDiff() override
	{
		bool result = m_buffer.LastDiff();
		if (result)
			ScrollToDiff(m_buffer.GetCurrentDiffIndex());
		return result;
	}

	bool NextDiff() override
	{
		bool result = m_buffer.NextDiff();
		if (result)
			ScrollToDiff(m_buffer.GetCurrentDiffIndex());
		return result;
	}

	bool PrevDiff() override
	{
		bool result = m_buffer.PrevDiff();
		if (result)
			ScrollToDiff(m_buffer.GetCurrentDiffIndex());
		return result;
	}

	bool FirstConflict() override
	{
		bool result = m_buffer.FirstConflict();
		if (result)
			ScrollToDiff(m_buffer.GetCurrentDiffIndex());
		return result;
	}

	bool LastConflict() override
	{
		bool result = m_buffer.LastConflict();
		if (result)
			ScrollToDiff(m_buffer.GetCurrentDiffIndex());
		return result;
	}

	bool NextConflict() override
	{
		bool result = m_buffer.NextConflict();
		if (result)
			ScrollToDiff(m_buffer.GetCurrentDiffIndex());
		return result;
	}

	bool PrevConflict() override
	{
		bool result = m_buffer.PrevConflict();
		if (result)
			ScrollToDiff(m_buffer.GetCurrentDiffIndex());
		return result;
	}

	bool SelectDiff(int diffIndex) override
	{
		bool result = m_buffer.SelectDiff(diffIndex);
		if (result)
			ScrollToDiff(m_buffer.GetCurrentDiffIndex());
		return result;
	}
	
	int  GetNextDiffIndex() const override
	{
		return m_buffer.GetNextDiffIndex();
	}

	int  GetPrevDiffIndex() const override
	{
		return m_buffer.GetPrevDiffIndex();
	}

	int  GetNextConflictIndex() const override
	{
		return m_buffer.GetNextConflictIndex();
	}

	int  GetPrevConflictIndex() const override
	{
		return m_buffer.GetPrevConflictIndex();
	}

	void CopyDiff(int diffIndex, int srcPane, int dstPane) override
	{
		m_buffer.CopyDiff(diffIndex, srcPane, dstPane);
		Invalidate();
	}

	void CopyDiffAll(int srcPane, int dstPane) override
	{
		m_buffer.CopyDiffAll(srcPane, dstPane);
		Invalidate();
	}

	int CopyDiff3Way(int dstPane) override
	{
		int result = m_buffer.CopyDiff3Way(dstPane);
		Invalidate();
		return result;
	}

	bool IsModified(int pane) const override
	{
		return m_buffer.IsModified(pane);
	}

	bool IsUndoable() const override
	{
		return m_buffer.IsUndoable();
	}

	bool IsCuttable() const override
	{
		int pane = GetActivePane();
		if (pane < 0)
			return false;
		return m_imgWindow[pane].IsRectanlgeSelectionVisible() && !GetReadOnly(pane);
	}

	bool IsCopyable() const override
	{
		int pane = GetActivePane();
		if (pane < 0)
			return false;
		return m_imgWindow[pane].IsRectanlgeSelectionVisible();
	}

	bool IsPastable() const override
	{
		int pane = GetActivePane();
		if (pane < 0)
			return false;
		return !!IsClipboardFormatAvailable(CF_DIB);
	}

	bool IsRedoable() const override
	{
		return m_buffer.IsRedoable();
	}

	bool IsCancellable() const override
	{
		bool cancellable = false;
		for (int pane = 0; pane < m_nImages; ++pane)
		{
			if (m_imgWindow[pane].IsRectanlgeSelectionVisible() ||
				m_imgWindow[pane].GetOverlappedImage().isValid())
				cancellable = true;
		}
		return cancellable;
	}

	bool IsRectangleSelectionVisible(int pane) const override
	{
		if (pane < 0 || pane >= m_nImages)
			return false;
		return m_imgWindow[pane].IsRectanlgeSelectionVisible();
	}

	RECT GetRectangleSelection(int pane) const override
	{
		if (pane < 0 || pane >= m_nImages)
			return {};
		return ConvertToRealRect(pane, m_imgWindow[pane].GetRectangleSelection(), false);
	}

	bool SelectAll() override
	{
		int pane = GetActivePane();
		if (pane < 0)
			return false;
		RECT rc = GetPreprocessedImageRect(pane);
		m_imgWindow[pane].SetRectangleSelection(rc.left, rc.top, rc.right, rc.bottom);
		m_imgWindow[pane].Invalidate();
		return true;
	}

	bool Copy() override
	{
		int pane = GetActivePane();
		if (pane < 0 || !m_imgWindow[pane].IsRectanlgeSelectionVisible())
			return false;
		Image image;
		RECT rc = ConvertToRealRect(pane, m_imgWindow[pane].GetRectangleSelection(), false);
		m_buffer.CopySubImage(pane, rc.left, rc.top, rc.right, rc.bottom, image);
		return !!image.getImage()->copyToClipboard(m_imgWindow[pane].GetHWND());
	}

	bool Cut() override
	{
		int pane = GetActivePane();
		if (pane < 0 || !m_imgWindow[pane].IsRectanlgeSelectionVisible())
			return false;
		Image image;
		RECT rc = ConvertToRealRect(pane, m_imgWindow[pane].GetRectangleSelection(), false);
		m_buffer.CopySubImage(pane, rc.left, rc.top, rc.right, rc.bottom, image);
		bool result = !!image.getImage()->copyToClipboard(m_imgWindow[pane].GetHWND());
		if (result)
		{
			m_buffer.DeleteRectangle(pane, rc.left, rc.top, rc.right, rc.bottom);
			Cancel();
		}
		return result;
	}

	bool Delete() override
	{
		int pane = GetActivePane();
		if (pane < 0 || !m_imgWindow[pane].IsRectanlgeSelectionVisible())
			return false;
		RECT rc = ConvertToRealRect(pane, m_imgWindow[pane].GetRectangleSelection(), false);
		Cancel();
		bool result = m_buffer.DeleteRectangle(pane, rc.left, rc.top, rc.right, rc.bottom);
		if (result)
			Invalidate();
		return result;
	}

	bool Paste() override
	{
		int pane = GetActivePane();
		if (pane < 0)
			return false;
		Cancel();
		CImgWindow& imgWindow = m_imgWindow[pane];
		fipImageEx image;
		image.pasteFromClipboard();
		image.convertTo32Bits();
		int maxwidth  = (std::max)(m_buffer.GetImageWidth(pane), static_cast<int>(image.getWidth()));
		int maxheight = (std::max)(m_buffer.GetImageHeight(pane), static_cast<int>(image.getHeight()));
		m_buffer.Resize(pane, maxwidth, maxheight);
		imgWindow.SetOverlappedImage(image);
		imgWindow.Invalidate();
		return true;
	}

	bool Undo() override
	{
		Cancel();
		bool result = m_buffer.Undo();
		if (result)
		{
			UpdateScrollBars();
			Invalidate();
		}
		return result;
	}

	bool Redo() override
	{
		Cancel();
		bool result = m_buffer.Redo();
		if (result)
		{
			UpdateScrollBars();
			Invalidate();
		}
		return result;
	}

	bool Cancel() override
	{
		if (!IsCancellable())
			return false;
		for (int pane = 0; pane < m_nImages; ++pane)
		{
			m_imgWindow[pane].DeleteRectangleSelection();
			m_imgWindow[pane].DeleteOverlappedImage();
		}
		Invalidate();
		return true;
	}

	void ScrollToDiff(int diffIndex)
	{
		if (!m_hWnd)
			return;
		if (diffIndex >= 0 && diffIndex < m_buffer.GetDiffCount())
		{
			Rect<int> rc = m_buffer.GetDiffInfo(diffIndex)->rc;
			for (int i = 0; i < m_nImages; ++i)
				m_imgWindow[i].ScrollTo(rc.left * m_buffer.GetDiffBlockSize(), rc.top * m_buffer.GetDiffBlockSize());
		}
		else
		{
			for (int i = 0; i < m_nImages; ++i)
				m_imgWindow[i].Invalidate();
		}

		Event evt;
		evt.eventType = SCROLLTODIFF;
		evt.diffIndex = diffIndex;
		notify(evt);
	}

	void ScrollTo(int x, int y, bool force = false)
	{
		if (!m_hWnd)
			return;
		for (int i = 0; i < m_nImages; ++i)
			m_imgWindow[i].ScrollTo(x, y, force);
		Event evt;
		evt.eventType = VSCROLL;
		notify(evt);
		evt.eventType = HSCROLL;
		notify(evt);
	}

	void Invalidate(bool erase = false)
	{
		if (!m_hWnd)
			return;
		if (m_nImages <= 1)
			return;
		for (int i = 0; i < m_nImages; ++i)
			m_imgWindow[i].Invalidate(erase);

		Event evt;
		evt.eventType = REFRESH;
		notify(evt);
	}

	bool NewImages(int nImages, int nPages, int width, int height) override
	{
		CloseImages();
		m_nImages = nImages;
		bool bSucceeded = m_buffer.NewImages(nImages, nPages, width, height);
		if (m_hWnd)
		{
			for (int i = 0; i < nImages; ++i)
			{
				m_imgWindow[i].Create(m_hInstance, m_hWnd);
				m_ChildWndProc[i] = (WNDPROC)SetWindowLongPtr(m_imgWindow[i].GetHWND(), GWLP_WNDPROC, (LONG_PTR)&ChildWndProc);
			}
		}
		m_buffer.CompareImages();
		if (m_hWnd)
		{
			std::vector<RECT> rects = CalcChildImgWindowRect(m_hWnd, nImages, m_bHorizontalSplit);
			for (int i = 0; i < nImages; ++i)
			{
				if (i < nImages - 1)
					m_imgWindow[i].SetScrollBar(m_bHorizontalSplit ? SB_VERT : SB_HORZ);
				m_imgWindow[i].SetWindowRect(rects[i]);
				m_imgWindow[i].SetImage(m_buffer.GetImage(i)->getFipImage());
				m_imgWindow[i].SetDarkBackgroundEnabled(s_bDarkBackgroundEnabled);
			}

			Event evt;
			evt.eventType = NEW;
			notify(evt);
		}
		return bSucceeded;
	}

	bool OpenImages(int nImages, const wchar_t * const filename[3])
	{
		CloseImages();
		m_nImages = nImages;
		bool bSucceeded = m_buffer.OpenImages(nImages, filename);
		if (m_hWnd)
		{
			for (int i = 0; i < nImages; ++i)
			{
				m_imgWindow[i].Create(m_hInstance, m_hWnd);
				m_ChildWndProc[i] = (WNDPROC)SetWindowLongPtr(m_imgWindow[i].GetHWND(), GWLP_WNDPROC, (LONG_PTR)&ChildWndProc);
			}
		}
		m_buffer.CompareImages();
		if (m_hWnd)
		{
			std::vector<RECT> rects = CalcChildImgWindowRect(m_hWnd, nImages, m_bHorizontalSplit);
			for (int i = 0; i < nImages; ++i)
			{
				if (i < nImages - 1)
					m_imgWindow[i].SetScrollBar(m_bHorizontalSplit ? SB_VERT : SB_HORZ);
				m_imgWindow[i].SetWindowRect(rects[i]);
				m_imgWindow[i].SetImage(m_buffer.GetImage(i)->getFipImage());
				m_imgWindow[i].SetDarkBackgroundEnabled(s_bDarkBackgroundEnabled);
			}

			Event evt;
			evt.eventType = OPEN;
			notify(evt);
		}
		return bSucceeded;
	}

	bool OpenImages(const wchar_t *filename1, const wchar_t *filename2) override
	{
		const wchar_t *filenames[] = {filename1, filename2};
		return OpenImages(2, filenames);
	}

	bool OpenImages(const wchar_t *filename1, const wchar_t *filename2, const wchar_t *filename3) override
	{
		const wchar_t *filenames[] = {filename1, filename2, filename3};
		return OpenImages(3, filenames);
	}

	bool ReloadImages() override
	{
		if (m_nImages == 2)
			return OpenImages(m_buffer.GetFileName(0), m_buffer.GetFileName(1));
		else if (m_nImages == 3)
			return OpenImages(m_buffer.GetFileName(0), m_buffer.GetFileName(1), m_buffer.GetFileName(2));
		return false;
	}

	bool SaveImage(int pane) override
	{
		return m_buffer.SaveImage(pane);
	}

	bool SaveImages() override
	{
		return m_buffer.SaveImages();
	}

	bool SaveImageAs(int pane, const wchar_t *filename) override
	{
		return m_buffer.SaveImageAs(pane, filename);
	}

	bool CloseImages() override
	{
		m_buffer.CloseImages();
		if (m_hWnd)
		{
			for (int i = 0; i < m_nImages; ++i)
				m_imgWindow[i].Destroy();
		}
		return true;
	}

	bool SaveDiffImageAs(int pane, const wchar_t *filename) override
	{
		return m_buffer.SaveDiffImageAs(pane, filename);
	}

	HWND GetPaneHWND(int pane) const override
	{
		if (pane < 0 || pane >= m_nImages || !m_hWnd)
			return NULL;
		return m_imgWindow[pane].GetHWND();
	}

	HWND GetHWND() const override
	{
		return m_hWnd;
	}

	int  GetImageWidth(int pane) const override
	{
		return m_buffer.GetImageWidth(pane);
	}

	int  GetImageHeight(int pane) const override
	{
		return m_buffer.GetImageHeight(pane);
	}

	int  GetDiffImageWidth() const
	{
		return m_buffer.GetDiffImageWidth();
	}

	int  GetDiffImageHeight() const
	{
		return m_buffer.GetDiffImageHeight();
	}

	int  GetImageBitsPerPixel(int pane) const override
	{
		return m_buffer.GetImageBitsPerPixel(pane);
	}

	int GetDiffIndexFromPoint(int x, int y) const
	{
		return m_buffer.GetDiffIndexFromPoint(x, y);
	}

	POINT GetImageOffset(int pane) const override
	{
		Point<unsigned> pt = m_buffer.GetImageOffset(pane);
		POINT pt2 = {static_cast<long>(pt.x), static_cast<long>(pt.y)};
		return pt2;
	}

	void AddImageOffset(int pane, int dx, int dy) override
	{
		m_buffer.AddImageOffset(pane, dx, dy);
		Invalidate();
	}

	Image *GetImage(int pane)
	{
		return m_buffer.GetImage(pane);
	}

	Image *GetDiffMapImage(unsigned w, unsigned h)
	{
		return m_buffer.GetDiffMapImage(w, h);
	}

	DRAGGING_MODE GetDraggingMode() const override
	{
		return m_draggingMode;
	}

	void SetDraggingMode(DRAGGING_MODE draggingMode) override
	{
		m_draggingMode = draggingMode;
		HCURSOR hCursor = GetMouseCursorFromDraggingMode(draggingMode);
		for (int pane = 0; pane < m_nImages; ++pane)
			m_imgWindow[pane].SetCursor(hCursor);
	}

	size_t GetMetadata(int pane, char *buf, size_t bufsize) const override
	{
		std::map<std::string, std::string> metadata = m_buffer.GetOriginalImage(pane)->getMetadata();
		std::string metadatastr;
		for (auto& it : metadata)
		{
			std::string value = it.second;
			wchar_t c = value.front();
			if ((c == '\'' || c == '"' || c == '>' || c == '|' || c == ':' || c == '{' || c == '\r' || c == '\n')
				|| value.find_first_of("\r\n") != std::string::npos || value.find(": ") != std::string::npos)
			{
				for (int i = static_cast<int>(value.length() - 1); i >= 0; --i)
				{
					if (value[i] == '\n' || (i < static_cast<int>(value.length()) - 1 && value[i] == '\r' && value[i + 1] != '\n'))
						value.insert(i + 1, "  ");
				}
				metadatastr += it.first + ": |\n  " + value + "\n";
			}
			else
				metadatastr += it.first + ": " + value + "\n";
		}
		if (buf)
		{
			if (bufsize >= metadatastr.length() + 1)
			{
				memcpy(buf, metadatastr.c_str(), metadatastr.length() + 1);
			}
			else if (bufsize > 0)
			{
				memcpy(buf, metadatastr.c_str(), bufsize - 1);
				buf[bufsize - 1] = '\0';
			}
		}
		return metadatastr.length() + 1;
	}

	BSTR ExtractTextFromImage(int pane, int page, OCR_RESULT_TYPE resultType) override
	{
		if (!m_pOcr)
			m_pOcr.reset(new ocr::COcr());
		if (!m_pOcr ||
			pane < 0 || pane >= m_buffer.GetPaneCount() ||
			page >= m_buffer.GetPageCount(pane))
			return nullptr;

		std::wstring text;
		int oldCurrentPage = m_buffer.GetCurrentPage(pane);
		int minpage, maxpage;
		if (page == -1)
		{
			minpage = 0;
			maxpage = m_buffer.GetPageCount(pane) - 1;
		}
		else
		{
			minpage = page;
			maxpage = page;
		}
		for (int p = minpage; p <= maxpage; ++p)
		{
			wchar_t* tmppath = nullptr;
			size_t bufsize = 0;
			_wdupenv_s(&tmppath, &bufsize, L"TEMP");
			wchar_t filename[MAX_PATH] = {};
			_snwprintf_s(filename, _TRUNCATE, L"%s/WinIMerge_ocr_%d_%d_%d.png",
				tmppath, GetCurrentProcessId(), pane, p);
			free(tmppath);
			m_buffer.SetCurrentPage(pane, p);
			Image image;
			m_buffer.CopySubImage(pane, 0, 0, m_buffer.GetImageWidth(pane), m_buffer.GetImageHeight(pane), image);
			image.save(filename);

			m_pOcr->load(filename);

			DeleteFile(filename);

			ocr::Result result;
			m_pOcr->extractText(result);

			wchar_t buf[256];

			if (resultType != OCR_RESULT_TYPE::TEXT_ONLY)
			{
				_snwprintf_s(buf, _TRUNCATE, L"- page: %d\n  content:\n", p + 1);
				text.append(buf);
			}
			for (auto&& line : result.lines)
			{
				switch (resultType)
				{
				case OCR_RESULT_TYPE::TEXT_ONLY:
					text.append(line.text);
					text.append(L"\n");
					break;
				case OCR_RESULT_TYPE::TEXT_PER_LINE_YAML:
				{
					ocr::Word& lastWord = line.words[line.words.size() - 1];
					auto minXi = std::min_element(line.words.begin(), line.words.end(),
						[](const ocr::Word& a, const ocr::Word& b) { return a.rect.x < b.rect.x; });
					auto minYi = std::min_element(line.words.begin(), line.words.end(),
						[](const ocr::Word& a, const ocr::Word& b) { return a.rect.y < b.rect.y; });
					auto maxYi = std::max_element(line.words.begin(), line.words.end(),
						[](const ocr::Word& a, const ocr::Word& b) { return a.rect.y + a.rect.height < b.rect.y + b.rect.height; });
					_snwprintf_s(buf, _TRUNCATE, L"  - rect: {x: %.0f, y: %.0f, w: %.0f, h: %.0f}\n    text: |\n      ",
						minXi->rect.x, minYi->rect.y,
						lastWord.rect.x + lastWord.rect.width - line.words[0].rect.x,
						maxYi->rect.y + maxYi->rect.height - minYi->rect.y);
					text.append(buf);
					text.append(line.text);
					text.append(L"\n");
					break;
				}
				case OCR_RESULT_TYPE::TEXT_PER_WORD_YAML:
					text.append(L"  -\n");
					for (auto&& word : line.words)
					{
						_snwprintf_s(buf, _TRUNCATE, L"    - rect: {x: %.0f, y: %.0f, w: %.0f, h: %.0f}\n      text: |\n        ",
							word.rect.x, word.rect.y,
							word.rect.width, word.rect.height);
						text.append(buf);
						text.append(word.text);
						text.append(L"\n");
					}
					break;
				}
			}
		}

		m_buffer.SetCurrentPage(pane, oldCurrentPage);

		return SysAllocStringLen(text.c_str(), static_cast<unsigned>(text.size()));
	}

	int GetSavePoint(int pane) const override
	{
		return m_buffer.GetSavePoint(pane);
	}

	void SetSavePoint(int pane, int pos) override
	{
		m_buffer.SetSavePoint(pane, pos);
	}

	bool IsSaveSupported(int pane) const override
	{
		return m_buffer.IsSaveSupported(pane);
	}

	int GetBlinkInterval() const override
	{
		return m_buffer.GetBlinkInterval();
	}

	void SetBlinkInterval(int interval) override
	{
		m_buffer.SetBlinkInterval(interval);
	}

	int GetOverlayAnimationInterval() const override
	{
		return m_buffer.GetOverlayAnimationInterval();
	}

	void SetOverlayAnimationInterval(int interval) override
	{
		m_buffer.SetOverlayAnimationInterval(interval);
	}

	bool IsDarkBackgroundEnabled() const
	{
		return s_bDarkBackgroundEnabled;
	}

	void SetDarkBackgroundEnabled(bool enabled)
	{
		if (s_bDarkBackgroundEnabled == enabled)
			return;
		s_bDarkBackgroundEnabled = enabled;
		DeleteObject(s_hbrBackground);
		s_hbrBackground = CreateSolidBrush(s_bDarkBackgroundEnabled ? RGB(0, 0, 0) : GetSysColor(COLOR_3DFACE));
		if (m_hWnd)
		{
			for (int pane = 0; pane < m_nImages; ++pane)
				m_imgWindow[pane].SetDarkBackgroundEnabled(s_bDarkBackgroundEnabled);
			SetClassLongPtr(m_hWnd, GCLP_HBRBACKGROUND, (LONG_PTR)s_hbrBackground);
			InvalidateRect(m_hWnd, NULL, TRUE);
		}
	}

	int GetLastErrorCode() const
	{
		return m_buffer.GetLastErrorCode();
	}

private:

	ATOM MyRegisterClass(HINSTANCE hInstance)
	{
		WNDCLASSEXW wcex = {0};
		wcex.cbSize         = sizeof(WNDCLASSEX); 
		wcex.style			= CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc	= (WNDPROC)CImgMergeWindow::WndProc;
		wcex.cbClsExtra		= 0;
		wcex.cbWndExtra		= 0;
		wcex.hInstance		= hInstance;
		wcex.hCursor        = LoadCursor (NULL, IDC_ARROW);
		wcex.hbrBackground = s_hbrBackground;
		wcex.lpszClassName	= L"WinImgMergeWindowClass";
		return RegisterClassExW(&wcex);
	}

	std::vector<RECT> CalcChildImgWindowRect(HWND hWnd, int nImages, bool bHorizontalSplit)
	{
		std::vector<RECT> childrects;
		RECT rcParent;
		GetClientRect(hWnd, &rcParent);
		RECT rc = rcParent;
		if (nImages > 0)
		{
			if (!bHorizontalSplit)
			{
				int cx = GetSystemMetrics(SM_CXVSCROLL);
				int width = (rcParent.left + rcParent.right - cx) / nImages - 2;
				rc.left = 0;
				rc.right = rc.left + width;
				for (int i = 0; i < nImages - 1; ++i)
				{
					childrects.push_back(rc);
					rc.left = rc.right + 2 * 2;
					rc.right = rc.left + width;
				}
				rc.right = rcParent.right;
				rc.left = rc.right - width - cx;
				childrects.push_back(rc);
			}
			else
			{
				int cy = GetSystemMetrics(SM_CXVSCROLL);
				int height = (rcParent.top + rcParent.bottom - cy) / nImages - 2;
				rc.top = 0;
				rc.bottom = rc.top + height;
				for (int i = 0; i < nImages - 1; ++i)
				{
					childrects.push_back(rc);
					rc.top = rc.bottom + 2 * 2;
					rc.bottom = rc.top + height;
				}
				rc.bottom = rcParent.bottom;
				rc.top = rc.bottom - height - cy;
				childrects.push_back(rc);
			}
		}
		return childrects;
	}

	void MoveSplitter(int x, int y)
	{
		RECT rcParent;
		GetClientRect(m_hWnd, &rcParent);

		RECT rc[3];
		for (int i = 0; i < m_nImages; ++i)
			rc[i] = m_imgWindow[i].GetWindowRect();

		if (!m_bHorizontalSplit)
		{
			int minx = rc[m_nDraggingSplitter].left + 32; 
			int maxx = rc[m_nDraggingSplitter + 1].right - 32; 
			if (x < minx)
				rc[m_nDraggingSplitter].right = minx;
			else if (x > maxx)
				rc[m_nDraggingSplitter].right = maxx;
			else
				rc[m_nDraggingSplitter].right = x;
			for (int i = m_nDraggingSplitter + 1; i < m_nImages; ++i)
			{
				int width   = rc[i].right - rc[i].left;
				rc[i].left  = rc[i - 1].right + 2 * 2;
				rc[i].right = rc[i].left + width;
			}
			rc[m_nImages - 1].right = rcParent.right;
		}
		else
		{
			rc[m_nDraggingSplitter].bottom = y;
			int miny = rc[m_nDraggingSplitter].top + 32; 
			int maxy = rc[m_nDraggingSplitter + 1].bottom - 32; 
			if (y < miny)
				rc[m_nDraggingSplitter].bottom = miny;
			else if (y > maxy)
				rc[m_nDraggingSplitter].bottom = maxy;
			else
				rc[m_nDraggingSplitter].bottom = y;
			for (int i = m_nDraggingSplitter + 1; i < m_nImages; ++i)
			{
				int height    = rc[i].bottom - rc[i].top;
				rc[i].top    = rc[i - 1].bottom + 2 * 2;
				rc[i].bottom = rc[i].top + height;
			}
			rc[m_nImages - 1].bottom = rcParent.bottom;
		}

		for (int i = 0; i < m_nImages; ++i)
			m_imgWindow[i].SetWindowRect(rc[i]);
	}

	void DrawXorBar(HDC hdc, int x1, int y1, int width, int height)
	{
		static const WORD _dotPatternBmp[8] = 
		{ 
			0x00aa, 0x0055, 0x00aa, 0x0055, 
			0x00aa, 0x0055, 0x00aa, 0x0055
		};

		HBITMAP hbm = CreateBitmap(8, 8, 1, 1, _dotPatternBmp);
		HBRUSH hbr = CreatePatternBrush(hbm);
		
		SetBrushOrgEx(hdc, x1, y1, 0);
		HBRUSH hbrushOld = (HBRUSH)SelectObject(hdc, hbr);
		
		PatBlt(hdc, x1, y1, width, height, PATINVERT);
		
		SelectObject(hdc, hbrushOld);
		
		DeleteObject(hbr);
		DeleteObject(hbm);
	}

	HCURSOR GetMouseCursorFromDraggingMode(DRAGGING_MODE draggingMode) const
	{
		TCHAR* cursor;
		switch (draggingMode)
		{
		case HORIZONTAL_WIPE:
			cursor = IDC_SIZEWE;
			break;
		case VERTICAL_WIPE:
			cursor = IDC_SIZENS;
			break;
		case RECTANGLE_SELECT:
			cursor = IDC_CROSS;
			break;
		default:
			cursor = IDC_ARROW;
			break;
		}
		return LoadCursor(nullptr, cursor);
	}

	void OnCreate(HWND hWnd, LPCREATESTRUCT lpCreateStruct)
	{
	}

	void OnSize(UINT nType, int cx, int cy)
	{
		std::vector<RECT> rects = CalcChildImgWindowRect(m_hWnd, m_nImages, m_bHorizontalSplit);
		for (int i = 0; i < m_nImages; ++i)
			m_imgWindow[i].SetWindowRect(rects[i]);
	}
	
	void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
	{
		if (nChar == VK_ESCAPE)
			Cancel();
	}

	void OnLButtonDown(UINT nFlags, int x, int y)
	{
		int i;
		for (i = 0; i < m_nImages - 1; ++i)
		{
			if (!m_bHorizontalSplit)
			{
				if (x < m_imgWindow[i + 1].GetWindowRect().left)
					break;
			}
			else
			{
				if (y < m_imgWindow[i + 1].GetWindowRect().top)
					break;
			}
		}
		m_oldSplitPosX = -4;
		m_oldSplitPosY = -4;
		m_nDraggingSplitter = i;
		SetCapture(m_hWnd);
	}

	void OnLButtonUp(UINT nFlags, int x, int y)
	{
		if (m_nDraggingSplitter == -1)
			return;
		ReleaseCapture();
		HDC hdc = GetWindowDC(m_hWnd);
		if (!m_bHorizontalSplit)
			DrawXorBar(hdc, m_oldSplitPosX - 2, 1, 4, m_imgWindow[0].GetWindowRect().bottom);
		else
			DrawXorBar(hdc, 1, m_oldSplitPosY - 2, m_imgWindow[0].GetWindowRect().right, 4);
		ReleaseDC(m_hWnd, hdc);
		MoveSplitter(x, y);
		m_nDraggingSplitter = -1;
	}

	void OnMouseMove(UINT nFlags, int x, int y)
	{
		if (m_nImages < 2)
			return;
		if (m_nDraggingSplitter == -1)
			return;
		HDC hdc = GetWindowDC(m_hWnd);
		if (!m_bHorizontalSplit)
		{
			DrawXorBar(hdc, m_oldSplitPosX - 2, 1, 4, m_imgWindow[0].GetWindowRect().bottom);
			DrawXorBar(hdc, x              - 2, 1, 4, m_imgWindow[0].GetWindowRect().bottom);
		}
		else
		{
			DrawXorBar(hdc, 1, m_oldSplitPosY - 2, m_imgWindow[0].GetWindowRect().right, 4);	
			DrawXorBar(hdc, 1, y              - 2, m_imgWindow[0].GetWindowRect().right, 4);	
		}
		m_oldSplitPosX = x;
		m_oldSplitPosY = y;
		ReleaseDC(m_hWnd, hdc);
	}

	void OnDestroy()
	{
		for (int i = 0; i < m_nImages; ++i)
		{
			if (m_ChildWndProc[i])
			{
				SetWindowLongPtr(m_imgWindow[i].GetHWND(), GWLP_WNDPROC, (LONG_PTR)m_ChildWndProc[i]);
				m_ChildWndProc[i] = NULL;
			}
		}
	}

	LRESULT OnWndMsg(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
	{
		switch (iMsg)
		{
		case WM_CREATE:
			OnCreate(hwnd, (LPCREATESTRUCT)lParam);
			break;
		case WM_COMMAND:
			PostMessage(GetParent(m_hWnd), iMsg, wParam, lParam);
			break;
		case WM_SIZE:
			OnSize((UINT)wParam, LOWORD(lParam), HIWORD(lParam));
			break;
		case WM_KEYDOWN:
			OnKeyDown((UINT)wParam, (int)(short)LOWORD(lParam), (UINT)HIWORD(lParam));
			break;
		case WM_LBUTTONDOWN:
			OnLButtonDown((UINT)(wParam), (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam));
			break;
		case WM_LBUTTONUP:
			OnLButtonUp((UINT)(wParam), (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam));
			break;
		case WM_MOUSEMOVE:
			OnMouseMove((UINT)(wParam), (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam));
			break;
		case WM_MOUSEWHEEL:
			PostMessage(m_imgWindow[0].GetHWND(), iMsg, wParam, lParam);
			break;
		case WM_SETCURSOR:
			SetCursor(::LoadCursor(nullptr, !m_bHorizontalSplit ? IDC_SIZEWE : IDC_SIZENS));
			break;
		case WM_TIMER:
		{
			auto now = std::chrono::system_clock::now();
			auto tse = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
			if (m_timerNext.count() == 0 || tse >= m_timerNext)
			{
				m_buffer.RefreshImages();
				if (m_nImages > 1)
				{
					for (int i = 0; i < m_nImages; ++i)
						m_imgWindow[i].Invalidate(false);
				}
				const auto actualCycle = tse - m_timerPrev;
				const auto idealCycle = 
					std::chrono::milliseconds((wParam == 2) ? m_buffer.GetOverlayAnimationInterval() / 20 : m_buffer.GetBlinkInterval() / 4);
				m_timerNext = tse +((m_timerNext.count() == 0 || actualCycle < idealCycle) ? idealCycle : actualCycle);
			}
			m_timerPrev = tse;
			break;
		}
		case WM_DESTROY:
			OnDestroy();
			break;
		default:
			return DefWindowProc(hwnd, iMsg, wParam, lParam);
		}
		return 0;
	}

	void notify(const Event &evt)
	{
		Event evt2 = evt;
		std::vector<EventListenerInfo>::iterator it;
		for (it = m_listener.begin(); it != m_listener.end(); ++it)
		{
			evt2.userdata = (*it).userdata;
			(*it).func(evt2);
		}
	}

	static LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
	{
		if (iMsg == WM_NCCREATE)
			SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<LPCREATESTRUCT>(lParam)->lpCreateParams));
		CImgMergeWindow *pImgWnd = reinterpret_cast<CImgMergeWindow *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
		LRESULT lResult = pImgWnd->OnWndMsg(hwnd, iMsg, wParam, lParam);
		return lResult;
	}

	RECT ConvertToRealRect(int pane, const RECT& rc, bool clamp = true) const
	{
		Point<int> ptRealStart, ptRealEnd;
		m_buffer.ConvertToRealPos(pane, rc.left, rc.top,
			ptRealStart.x, ptRealStart.y, clamp);
		m_buffer.ConvertToRealPos(pane, rc.right, rc.bottom,
			ptRealEnd.x, ptRealEnd.y, clamp);
		return { ptRealStart.x, ptRealStart.y, ptRealEnd.x, ptRealEnd.y };
	}

	void UpdateScrollBars()
	{
		for (int i = 0; i < m_nImages; ++i)
			m_imgWindow[i].UpdateScrollBars();
	}

	void PasteOverlappedImage(int pane)
	{
		const CImgWindow& imgWindow = m_imgWindow[pane];
		if (!imgWindow.GetOverlappedImage().isValid())
			return;
		RECT rcOverlapeedImage = ConvertToRealRect(pane, imgWindow.GetOverlappedImageRect(), false);
		Image image(imgWindow.GetOverlappedImage());
		m_buffer.PasteImage(pane, rcOverlapeedImage.left, rcOverlapeedImage.top, image);
		UpdateScrollBars();
		Invalidate();
	}

	void PasteAndDeleteOverlappedImage(int pane)
	{
		PasteOverlappedImage(pane);
		m_imgWindow[pane].DeleteOverlappedImage();
		Invalidate();
	}

	RECT GetPreprocessedImageRect(int pane) const
	{
		Point<unsigned int> ptOffset = m_buffer.GetImageOffset(pane);
		long right = m_buffer.GetPreprocessedImageWidth(pane) + ptOffset.x;
		long bottom = m_buffer.GetPreprocessedImageHeight(pane) + ptOffset.y;
		return { static_cast<long>(ptOffset.x), static_cast<long>(ptOffset.y), right, bottom };
	}

	RECT GetRightBoxRect(int pane) const
	{
		RECT rc = GetPreprocessedImageRect(pane);
		return { rc.right, rc.top, rc.right + 8, rc.bottom };
	}

	RECT GetBottomBoxRect(int pane) const
	{
		RECT rc = GetPreprocessedImageRect(pane);
		return { rc.left, rc.bottom, rc.right, rc.bottom + 8 };
	}

	RECT GetRightBottomBoxRect(int pane) const
	{
		RECT rc = GetPreprocessedImageRect(pane);
		return { rc.right, rc.bottom, rc.right + 8, rc.bottom + 8 };
	}

	void ChildWnd_OnKeyDown(HWND hwnd, const Event& evt)
	{
		if (evt.keycode == VK_ESCAPE)
			Cancel();
		else if (evt.keycode == VK_RETURN)
			PasteAndDeleteOverlappedImage(evt.pane);
	}

	void ChildWnd_OnLButtonDown(HWND hwnd, const Event& evt)
	{
		m_bDragging = true;
		m_draggingModeCurrent = m_draggingMode;
		m_ptOrg.x = evt.x;
		m_ptOrg.y = evt.y;
		m_ptPrev.x = INT_MIN;
		m_ptPrev.y = INT_MIN;
		SetCapture(hwnd);
		POINT pt = GetCursorPos(evt.pane);
		CImgWindow& imgWindow = m_imgWindow[evt.pane];
		RECT rcSelection = imgWindow.GetRectangleSelection();
		RECT rcOverlapeedImage = imgWindow.GetOverlappedImageRect();
		RECT rcPreprocessedImage = GetPreprocessedImageRect(evt.pane);

		if (imgWindow.GetOverlappedImage().isValid() && !PtInRect(&rcOverlapeedImage, pt))
			PasteAndDeleteOverlappedImage(evt.pane);
		if (imgWindow.IsRectanlgeSelectionVisible() && !PtInRect(&rcSelection, pt))
			imgWindow.DeleteRectangleSelection();

		if (PtInRect(&GetRightBottomBoxRect(evt.pane), pt))
		{
			imgWindow.SetRectangleSelection(
				rcPreprocessedImage.left, rcPreprocessedImage.top,
				rcPreprocessedImage.right, rcPreprocessedImage.bottom);
			m_draggingModeCurrent = DRAGGING_MODE::RESIZE_BOTH;
		}
		else if (PtInRect(&GetRightBoxRect(evt.pane), pt))
		{
			imgWindow.SetRectangleSelection(
				rcPreprocessedImage.left, rcPreprocessedImage.top,
				rcPreprocessedImage.right, rcPreprocessedImage.bottom);
			m_draggingModeCurrent = DRAGGING_MODE::RESIZE_WIDTH;
		}
		else if (PtInRect(&GetBottomBoxRect(evt.pane), pt))
		{
			imgWindow.SetRectangleSelection(
				rcPreprocessedImage.left, rcPreprocessedImage.top,
				rcPreprocessedImage.right, rcPreprocessedImage.bottom);
			m_draggingModeCurrent = DRAGGING_MODE::RESIZE_HEIGHT;
		}
		else if (imgWindow.GetOverlappedImage().isValid() && PtInRect(&rcOverlapeedImage, pt))
		{
			bool controlKeyPressed = (GetAsyncKeyState(VK_CONTROL) & 0x8000);
			if (controlKeyPressed)
				PasteOverlappedImage(evt.pane);
			imgWindow.RestartDraggingOverlappedImage(pt);
			m_draggingModeCurrent = DRAGGING_MODE::MOVE_IMAGE;
		}
		else if ((imgWindow.IsRectanlgeSelectionVisible() && PtInRect(&rcSelection, pt)))
		{
			m_draggingModeCurrent = DRAGGING_MODE::MOVE_IMAGE;
		}
		else if (m_draggingModeCurrent == DRAGGING_MODE::VERTICAL_WIPE)
		{
			imgWindow.SetRectangleSelection(0, pt.y, m_buffer.GetImageWidth(evt.pane), pt.y);
			m_buffer.SetWipeModePosition(CImgDiffBuffer::WIPE_VERTICAL, pt.y);
		}
		else if (m_draggingModeCurrent == DRAGGING_MODE::HORIZONTAL_WIPE)
		{
			imgWindow.SetRectangleSelection(pt.x, 0, pt.x, m_buffer.GetImageHeight(evt.pane));
			m_buffer.SetWipeModePosition(CImgDiffBuffer::WIPE_HORIZONTAL, pt.x);
		}
		else if (m_draggingModeCurrent == DRAGGING_MODE::RECTANGLE_SELECT)
		{
			imgWindow.SetRectangleSelectionStart(pt.x, pt.y);
			imgWindow.SetRectangleSelectionEnd(pt.x, pt.y);
		}
		Invalidate();
	}

	void ChildWnd_OnLButtonUp(HWND hwnd, const Event& evt)
	{
		if (!m_bDragging)
			return;
		m_bDragging = false;
		ReleaseCapture();
		switch (m_draggingModeCurrent)
		{
		case DRAGGING_MODE::ADJUST_OFFSET:
		{
			POINT ptOffset = GetImageOffset(evt.pane);
			double zoom = GetZoom();
			m_imgWindow[evt.pane].DeleteRectangleSelection();
			AddImageOffset(evt.pane, static_cast<int>((evt.x - m_ptOrg.x) / zoom), static_cast<int>((evt.y - m_ptOrg.y) / zoom));
			break;
		}
		case DRAGGING_MODE::VERTICAL_WIPE:
		case DRAGGING_MODE::HORIZONTAL_WIPE:
		{
			m_buffer.SetWipeMode(CImgDiffBuffer::WIPE_NONE);
			m_imgWindow[evt.pane].DeleteRectangleSelection();
			Invalidate();
			break;
		}
		case DRAGGING_MODE::RECTANGLE_SELECT:
		{
			if (memcmp(
				&m_imgWindow[evt.pane].GetRectangleSelectionStart(),
				&m_imgWindow[evt.pane].GetRectangleSelectionEnd(), sizeof POINT) == 0)
			{
				m_imgWindow[evt.pane].DeleteRectangleSelection();
				Invalidate();
			}
			break;
		}
		case DRAGGING_MODE::RESIZE_WIDTH:
		case DRAGGING_MODE::RESIZE_HEIGHT:
		case DRAGGING_MODE::RESIZE_BOTH:
		{
			m_imgWindow[evt.pane].DeleteRectangleSelection();
			POINT pt = GetCursorPos(evt.pane);
			RECT rc = GetPreprocessedImageRect(evt.pane);
			int width = m_buffer.GetImageWidth(evt.pane);
			int height = m_buffer.GetImageHeight(evt.pane);
			if (m_draggingModeCurrent == DRAGGING_MODE::RESIZE_WIDTH || 
			    m_draggingModeCurrent == DRAGGING_MODE::RESIZE_BOTH)
				width += pt.x - rc.right;
			if (m_draggingModeCurrent == DRAGGING_MODE::RESIZE_HEIGHT || 
			    m_draggingModeCurrent == DRAGGING_MODE::RESIZE_BOTH)
				height += pt.y - rc.bottom;
			if (width > 0 && height > 0)
			{
				m_buffer.Resize(evt.pane, width, height);
			}
			Invalidate();
			break;
		}
		}
	}

	void ChildWnd_OnMouseMove(HWND hwnd, const Event& evt)
	{
		CImgWindow& imgWindow = m_imgWindow[evt.pane];
		POINT pt = GetCursorPos(evt.pane);
		if (!m_bDragging)
		{
			RECT rcSelect = imgWindow.GetRectangleSelection();
			RECT rcOverlappedImage = imgWindow.GetOverlappedImageRect();
			if ((imgWindow.IsRectanlgeSelectionVisible() && PtInRect(&rcSelect, pt)) ||
				(imgWindow.GetOverlappedImage().isValid() && PtInRect(&rcOverlappedImage, pt)))
				imgWindow.SetCursor(LoadCursor(nullptr, IDC_SIZEALL));
			else if (PtInRect(&GetRightBottomBoxRect(evt.pane), pt))
				imgWindow.SetCursor(LoadCursor(nullptr, IDC_SIZENWSE));
			else if (PtInRect(&GetRightBoxRect(evt.pane), pt))
				imgWindow.SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
			else if (PtInRect(&GetBottomBoxRect(evt.pane), pt))
				imgWindow.SetCursor(LoadCursor(nullptr, IDC_SIZENS));
			else
				imgWindow.SetCursor(GetMouseCursorFromDraggingMode(m_draggingMode));
			return;
		}
		double zoom = GetZoom();
		if (m_draggingModeCurrent == DRAGGING_MODE::MOVE)
		{
			SCROLLINFO sih{ sizeof SCROLLINFO, SIF_RANGE | SIF_PAGE };
			GetScrollInfo(hwnd, SB_HORZ, &sih);
			if (sih.nMax > static_cast<int>(sih.nPage))
			{
				int posx = GetScrollPos(hwnd, SB_HORZ) + static_cast<int>((m_ptOrg.x - evt.x) * zoom);
				if (posx < 0)
					posx = 0;
				SendMessage(hwnd, WM_HSCROLL, MAKEWPARAM(SB_THUMBTRACK, posx), 0);
				m_ptOrg.x = evt.x;
			}

			SCROLLINFO siv{ sizeof SCROLLINFO, SIF_RANGE | SIF_PAGE };
			GetScrollInfo(hwnd, SB_VERT, &siv);
			if (siv.nMax > static_cast<int>(siv.nPage))
			{
				int posy = GetScrollPos(hwnd, SB_VERT) + static_cast<int>((m_ptOrg.y - evt.y) * zoom);
				if (posy < 0)
					posy = 0;
				SendMessage(hwnd, WM_VSCROLL, MAKEWPARAM(SB_THUMBTRACK, posy), 0);
				m_ptOrg.y = evt.y;
			}
		}
		else if (m_draggingModeCurrent == DRAGGING_MODE::ADJUST_OFFSET)
		{
			RECT rc = GetPreprocessedImageRect(evt.pane);
			int offsetX = rc.left + static_cast<int>((evt.x - m_ptOrg.x) / zoom);
			int offsetY = rc.top + static_cast<int>((evt.y - m_ptOrg.y) / zoom);
			imgWindow.SetRectangleSelection(offsetX, offsetY,
				offsetX + rc.right - rc.left, offsetY + rc.bottom - rc.top, false);
			m_ptPrev.x = evt.x;
			m_ptPrev.y = evt.y;
		}
		else if (m_draggingModeCurrent == DRAGGING_MODE::VERTICAL_WIPE)
		{
			int oldWipePos = m_buffer.GetWipePosition();
			imgWindow.SetRectangleSelection(0, pt.y, m_buffer.GetImageWidth(evt.pane), pt.y);
			m_buffer.SetWipePosition(pt.y);
			if (m_buffer.GetWipePosition() != oldWipePos)
			{
				bool anyTransparent = m_buffer.IsAnyPaneTransparent();
				for (int i = 0; i < m_nImages; ++i)
				{
					if (m_buffer.IsPaneTransparent(i) || (anyTransparent && m_nImages >= 3))
						RedrawWindow(m_imgWindow[i].GetHWND(), NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE);
				}
				Invalidate();
			}
		}
		else if (m_draggingModeCurrent == DRAGGING_MODE::HORIZONTAL_WIPE)
		{
			int oldWipePos = m_buffer.GetWipePosition();
			imgWindow.SetRectangleSelection(pt.x, 0, pt.x, m_buffer.GetImageHeight(evt.pane));
			m_buffer.SetWipePosition(pt.x);
			if (m_buffer.GetWipePosition() != oldWipePos)
			{
				bool anyTransparent = m_buffer.IsAnyPaneTransparent();
				for (int i = 0; i < m_nImages; ++i)
				{
					if (m_buffer.IsPaneTransparent(i) || (anyTransparent && m_nImages >= 3))
						RedrawWindow(m_imgWindow[i].GetHWND(), NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE);
				}
				Invalidate();
			}
		}
		else if (m_draggingModeCurrent == DRAGGING_MODE::RECTANGLE_SELECT)
		{
			imgWindow.SetRectangleSelectionEnd(pt.x, pt.y);
			Invalidate();
		}
		else if (m_draggingModeCurrent == DRAGGING_MODE::MOVE_IMAGE)
		{
			if (imgWindow.IsRectanlgeSelectionVisible())
			{
				RECT rcSelect = imgWindow.GetRectangleSelection();
				RECT rcSelectReal = ConvertToRealRect(evt.pane, rcSelect, false);
				bool controlKeyPressed = (GetAsyncKeyState(VK_CONTROL) & 0x8000);
				Image image;
				m_buffer.CopySubImage(evt.pane, rcSelectReal.left, rcSelectReal.top,
					rcSelectReal.right, rcSelectReal.bottom, image);
				imgWindow.DeleteRectangleSelection();
				imgWindow.StartDraggingOverlappedImage(*image.getFipImage(),
					{ rcSelect.left, rcSelect.top }, pt);
				if (!controlKeyPressed)
				{
					m_buffer.DeleteRectangle(evt.pane,
						rcSelectReal.left, rcSelectReal.top,
						rcSelectReal.right, rcSelectReal.bottom);
				}
			}
			else
			{
				imgWindow.DragOverlappedImage(pt);
			}
			imgWindow.Invalidate();
		}
		else if (m_draggingModeCurrent == DRAGGING_MODE::RESIZE_WIDTH)
		{
			RECT rc = GetPreprocessedImageRect(evt.pane);
			imgWindow.SetRectangleSelectionEnd(pt.x, rc.bottom, false);
			Invalidate();
		}
		else if (m_draggingModeCurrent == DRAGGING_MODE::RESIZE_HEIGHT)
		{
			RECT rc = GetPreprocessedImageRect(evt.pane);
			imgWindow.SetRectangleSelectionEnd(rc.right, pt.y, false);
			Invalidate();
		}
		else if (m_draggingModeCurrent == DRAGGING_MODE::RESIZE_BOTH)
		{
			imgWindow.SetRectangleSelectionEnd(pt.x, pt.y, false);
			Invalidate();
		}
	}

	void ChildWnd_OnLButtonDblClk(HWND hwnd, const Event& evt)
	{
		POINT pt = GetCursorPos(evt.pane);
		int diffIndex = GetDiffIndexFromPoint(pt.x, pt.y);
		if (diffIndex >= 0)
			SelectDiff(diffIndex);
		else
			SelectDiff(-1);
	}

	void ChildWnd_OnKillFocus(HWND hwnd, const Event& evt)
	{
		PasteAndDeleteOverlappedImage(evt.pane);
	}

	void ChildWnd_OnHVScroll(HWND hwnd, int iMsg, WPARAM wParam, LPARAM lParam, const Event& evt)
	{
		switch (iMsg)
		{
		case WM_HSCROLL:
		case WM_VSCROLL:
			if (LOWORD(wParam) == SB_THUMBTRACK)
			{
				SCROLLINFO si{ sizeof SCROLLINFO, SIF_TRACKPOS };
				GetScrollInfo(hwnd, (iMsg == WM_HSCROLL) ? SB_HORZ : SB_VERT, &si);
				wParam |= (si.nTrackPos & 0xff0000) >> 8;
			}
			// [[fallthrough]]
		case WM_MOUSEWHEEL:
			POINT ptLP = m_imgWindow[evt.pane].GetCursorPos();
			POINT ptDP;
			::GetCursorPos(&ptDP);
			RECT rc;
			::GetWindowRect(m_imgWindow[evt.pane].GetHWND(), &rc);
			ptDP.x -= rc.left;
			ptDP.y -= rc.top;
			for (int j = 0; j < m_nImages; ++j)
			{
				(m_ChildWndProc[j])(m_imgWindow[j].GetHWND(), iMsg, wParam, lParam);
				if (GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL)
					m_imgWindow[j].ScrollTo2(ptLP.x, ptLP.y, ptDP.x, ptDP.y);
			}
			break;
		}
	}

	static LRESULT CALLBACK ChildWndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
	{
		Event evt;
		int i;
		CImgMergeWindow *pImgWnd = reinterpret_cast<CImgMergeWindow *>(GetWindowLongPtr(GetParent(hwnd), GWLP_USERDATA));
		for (i = 0; i < pImgWnd->m_nImages; ++i)
			if (pImgWnd->m_imgWindow[i].GetHWND() == hwnd)
				break;
		evt.pane = i;
		evt.flags = (unsigned)wParam; 
		evt.x = (int)(short)LOWORD(lParam);
		evt.y = (int)(short)HIWORD(lParam);
		switch(iMsg)
		{
		case WM_LBUTTONDOWN:
			evt.eventType = LBUTTONDOWN; goto NEXT;
		case WM_LBUTTONUP:
			evt.eventType = LBUTTONUP; goto NEXT;
		case WM_LBUTTONDBLCLK:
			evt.eventType = LBUTTONDBLCLK; goto NEXT;
		case WM_RBUTTONDOWN:
			evt.eventType = RBUTTONDOWN; goto NEXT;
		case WM_RBUTTONUP:
			evt.eventType = RBUTTONUP; goto NEXT;
		case WM_RBUTTONDBLCLK:
			evt.eventType = RBUTTONDBLCLK; goto NEXT;
		case WM_MOUSEMOVE:
			evt.eventType = MOUSEMOVE; goto NEXT;
		case WM_MOUSEWHEEL:
			evt.flags = GET_KEYSTATE_WPARAM(wParam);
			evt.eventType = MOUSEWHEEL;
			evt.delta = GET_WHEEL_DELTA_WPARAM(wParam);
			goto NEXT;
		case WM_KEYDOWN:
			evt.eventType = KEYDOWN; evt.keycode = static_cast<int>(wParam); goto NEXT;
		case WM_KEYUP:
			evt.eventType = KEYUP; evt.keycode = static_cast<int>(wParam); goto NEXT;
		case WM_CONTEXTMENU:
			evt.eventType = CONTEXTMENU; goto NEXT;
		case WM_SIZE:
			evt.eventType = SIZE; evt.width = LOWORD(lParam); evt.height = HIWORD(wParam); goto NEXT;
		case WM_HSCROLL:
			evt.eventType = HSCROLL;goto NEXT;
		case WM_VSCROLL:
			evt.eventType = VSCROLL; goto NEXT;
		case WM_SETFOCUS:
			evt.eventType = SETFOCUS; goto NEXT;
		case WM_KILLFOCUS:
			evt.eventType = KILLFOCUS; goto NEXT;
		NEXT:
			pImgWnd->notify(evt);
			break;
		}
		switch (iMsg)
		{
		case WM_KEYDOWN:
			pImgWnd->ChildWnd_OnKeyDown(hwnd, evt);
			break;
		case WM_LBUTTONDOWN:
			pImgWnd->ChildWnd_OnLButtonDown(hwnd, evt);
			break;
		case WM_LBUTTONUP:
			pImgWnd->ChildWnd_OnLButtonUp(hwnd, evt);
			break;
		case WM_MOUSEMOVE:
			pImgWnd->ChildWnd_OnMouseMove(hwnd, evt);
			break;
		case WM_LBUTTONDBLCLK:
			pImgWnd->ChildWnd_OnLButtonDblClk(hwnd, evt);
			break;
		case WM_KILLFOCUS:
			pImgWnd->ChildWnd_OnKillFocus(hwnd, evt);
			break;
		case WM_HSCROLL:
		case WM_VSCROLL:
		case WM_MOUSEWHEEL:
			pImgWnd->ChildWnd_OnHVScroll(hwnd, iMsg, wParam, lParam, evt);
			return 0;
		}
		return (pImgWnd->m_ChildWndProc[i])(hwnd, iMsg, wParam, lParam);
	}

	int m_nImages;
	HWND m_hWnd;
	HINSTANCE m_hInstance;
	inline static HBRUSH s_hbrBackground = CreateSolidBrush(GetSysColor(COLOR_3DFACE));
	CImgWindow m_imgWindow[3];
	WNDPROC m_ChildWndProc[3];
	std::vector<EventListenerInfo> m_listener;
	int m_nDraggingSplitter;
	bool m_bHorizontalSplit;
	int m_oldSplitPosX;
	int m_oldSplitPosY;
	bool m_bDragging;
	POINT m_ptOrg;
	POINT m_ptPrev;
	DRAGGING_MODE m_draggingMode;
	DRAGGING_MODE m_draggingModeCurrent;
	CImgMergeBuffer m_buffer;
	ULONG_PTR m_gdiplusToken;
	std::unique_ptr<ocr::COcr> m_pOcr;
	std::chrono::milliseconds m_timerPrev;
	std::chrono::milliseconds m_timerNext;
	inline static bool s_bDarkBackgroundEnabled = false;
};
