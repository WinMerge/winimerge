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
#include "FreeImagePlus.h"
#include "ImgWindow.hpp"
#include "ImgMergeBuffer.hpp"
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
		, m_gdiplusToken(0)
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

	const wchar_t *GetFileName(int pane)
	{
		return m_buffer.GetFileName(pane);
	}

	int GetPaneCount() const
	{
		return m_nImages;
	}

	RECT GetPaneWindowRect(int pane) const
	{
		if (pane < 0 || pane >= m_nImages || !m_hWnd)
		{
			RECT rc = {-1, -1, -1, -1};
			return rc;
		}
		return m_imgWindow[pane].GetWindowRect();
	}

	RECT GetWindowRect() const
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

	bool SetWindowRect(const RECT& rc)
	{
		if (m_hWnd)
			MoveWindow(m_hWnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
		return true;
	}

	POINT GetCursorPos(int pane) const
	{
		POINT pt = {-1, -1};
		if (pane < 0 || pane > m_nImages || !m_hWnd)
			return pt;
		return m_imgWindow[pane].GetCursorPos();
	}

	bool ConvertToRealPos(int pane, const POINT& pt, POINT& ptReal) const
	{
		int rx, ry;
		bool result = m_buffer.ConvertToRealPos(pane, pt.x, pt.y, rx, ry);
		ptReal = { rx, ry };
		return result;
	}

	RGBQUAD GetPixelColor(int pane, int x, int y) const
	{
		return m_buffer.GetPixelColor(pane, x, y);
	}

	double GetColorDistance(int pane1, int pane2, int x, int y) const
	{
		return m_buffer.GetColorDistance(pane1, pane2, x, y);
	}

	int GetActivePane() const
	{
		if (!m_hWnd)
			return -1;
		for (int i = 0; i < m_nImages; ++i)
			if (m_imgWindow[i].IsFocused())
				return i;
		return -1;
	}

	void SetActivePane(int pane)
	{
		if (pane < 0 || pane >= m_nImages || !m_hWnd)
			return;
		m_imgWindow[pane].SetFocus();
	}

	bool GetReadOnly(int pane) const
	{
		return m_buffer.GetReadOnly(pane);
	}

	void SetReadOnly(int pane, bool readOnly)
	{
		m_buffer.SetReadOnly(pane, readOnly);
	}

	bool GetHorizontalSplit() const
	{
		return m_bHorizontalSplit;
	}

	void SetHorizontalSplit(bool horizontalSplit)
	{
		if (!m_hWnd)
			return;
		m_bHorizontalSplit = horizontalSplit;
		std::vector<RECT> rects = CalcChildImgWindowRect(m_hWnd, m_nImages, m_bHorizontalSplit);
		for (int i = 0; i < m_nImages; ++i)
			m_imgWindow[i].SetWindowRect(rects[i]);
	}

	COLORREF GetDiffColor() const
	{
		return RGBQUADtoCOLORREF(m_buffer.GetDiffColor());
	}

	void SetDiffColor(COLORREF clrDiffColor)
	{
		m_buffer.SetDiffColor(COLORREFtoRGBQUAD(clrDiffColor));
		Invalidate();
	}

	COLORREF GetDiffDeletedColor() const
	{
		return RGBQUADtoCOLORREF(m_buffer.GetDiffDeletedColor());
	}

	void SetDiffDeletedColor(COLORREF clrDiffDeletedColor)
	{
		m_buffer.SetDiffDeletedColor(COLORREFtoRGBQUAD(clrDiffDeletedColor));
		Invalidate();
	}

	COLORREF GetSelDiffColor() const
	{
		return RGBQUADtoCOLORREF(m_buffer.GetSelDiffColor());
	}

	void SetSelDiffColor(COLORREF clrSelDiffColor)
	{
		m_buffer.SetSelDiffColor(COLORREFtoRGBQUAD(clrSelDiffColor));
		Invalidate();
	}

	COLORREF GetSelDiffDeletedColor() const
	{
		return RGBQUADtoCOLORREF(m_buffer.GetSelDiffDeletedColor());
	}

	void SetSelDiffDeletedColor(COLORREF clrSelDiffDeletedColor)
	{
		m_buffer.SetSelDiffColor(COLORREFtoRGBQUAD(clrSelDiffDeletedColor));
		Invalidate();
	}

	double GetDiffColorAlpha() const
	{
		return m_buffer.GetDiffColorAlpha();
	}

	void SetDiffColorAlpha(double diffColorAlpha)
	{
		m_buffer.SetDiffColorAlpha(diffColorAlpha);
		Invalidate();
	}

	RGBQUAD GetBackColor() const
	{
		return m_hWnd ? m_imgWindow[0].GetBackColor() : RGBQUAD{0};
	}

	void SetBackColor(RGBQUAD backColor)
	{
		if (m_hWnd)
		{
			for (int i = 0; i < 3; ++i)
				m_imgWindow[i].SetBackColor(backColor);
		}
	}

	bool GetUseBackColor() const
	{
		return m_hWnd ? m_imgWindow[0].GetUseBackColor() : false;
	}

	void SetUseBackColor(bool useBackColor)
	{
		if (!m_hWnd)
			return;
		for (int i = 0; i < 3; ++i)
			m_imgWindow[i].SetUseBackColor(useBackColor);
	}

	double GetZoom() const
	{
		return m_hWnd ? m_imgWindow[0].GetZoom() : 1.0;
	}

	void SetZoom(double zoom)
	{
		if (!m_hWnd)
			return;
		for (int i = 0; i < 3; ++i)
			m_imgWindow[i].SetZoom(zoom);

		Event evt;
		evt.eventType = REFRESH;
		notify(evt);
	}

	int  GetCurrentPage(int pane) const
	{
		return m_buffer.GetCurrentPage(pane);
	}

	void SetCurrentPage(int pane, int page)
	{
		m_buffer.SetCurrentPage(pane, page);
		Invalidate();
	}

	void SetCurrentPageAll(int page)
	{
		m_buffer.SetCurrentPageAll(page);
		Invalidate();
	}

	int  GetCurrentMaxPage() const
	{
		return m_buffer.GetCurrentMaxPage();
	}

	int  GetPageCount(int pane) const
	{
		return m_buffer.GetPageCount(pane);
	}

	int  GetMaxPageCount() const
	{
		return m_buffer.GetMaxPageCount();
	}

	double GetColorDistanceThreshold() const
	{
		return m_buffer.GetColorDistanceThreshold();
	}

	void SetColorDistanceThreshold(double threshold)
	{
		m_buffer.SetColorDistanceThreshold(threshold);
		Invalidate();
	}

	int  GetDiffBlockSize() const
	{
		return m_buffer.GetDiffBlockSize();
	}
	
	void SetDiffBlockSize(int blockSize)
	{
		m_buffer.SetDiffBlockSize(blockSize);
		Invalidate();
	}

	INSERTION_DELETION_DETECTION_MODE GetInsertionDeletionDetectionMode() const
	{
		return static_cast<INSERTION_DELETION_DETECTION_MODE>(m_buffer.GetInsertionDeletionDetectionMode());
	}

	void SetInsertionDeletionDetectionMode(INSERTION_DELETION_DETECTION_MODE insertionDeletinoMode)
	{
		m_buffer.SetInsertionDeletionDetectionMode(static_cast<CImgMergeBuffer::INSERTION_DELETION_DETECTION_MODE>(insertionDeletinoMode));
		Invalidate();
	}

	OVERLAY_MODE GetOverlayMode() const
	{
		return static_cast<OVERLAY_MODE>(m_buffer.GetOverlayMode());
	}

	void SetOverlayMode(OVERLAY_MODE overlayMode)
	{
		m_buffer.SetOverlayMode(static_cast<CImgMergeBuffer::OVERLAY_MODE>(overlayMode));
		Invalidate();
		if (overlayMode == OVERLAY_ALPHABLEND_ANIM)
			SetTimer(m_hWnd, 2, 50, NULL);
		else
			KillTimer(m_hWnd, 2);
	}

	double GetOverlayAlpha() const
	{
		return m_buffer.GetOverlayAlpha();
	}

	void SetOverlayAlpha(double overlayAlpha)
	{
		m_buffer.SetOverlayAlpha(overlayAlpha);
		Invalidate();
	}

	bool GetShowDifferences() const
	{
		return m_buffer.GetShowDifferences();
	}

	void SetShowDifferences(bool visible)
	{
		m_buffer.SetShowDifferences(visible);
		Invalidate();
	}

	bool GetBlinkDifferences() const
	{
		return m_buffer.GetBlinkDifferences();
	}

	void SetBlinkDifferences(bool blink)
	{
		m_buffer.SetBlinkDifferences(blink);
		Invalidate();
		if (blink)
			SetTimer(m_hWnd, 1, 400, NULL);
		else
			KillTimer(m_hWnd, 1);
	}

	float GetVectorImageZoomRatio() const
	{
		return m_buffer.GetVectorImageZoomRatio();
	}

	void SetVectorImageZoomRatio(float zoom)
	{
		m_buffer.SetVectorImageZoomRatio(zoom);
		Invalidate();
	}

	int  GetDiffCount() const
	{
		return m_buffer.GetDiffCount();
	}

	int  GetConflictCount() const
	{
		return m_buffer.GetConflictCount();
	}

	int  GetCurrentDiffIndex() const
	{
		return m_buffer.GetCurrentDiffIndex();
	}

	bool FirstDiff()
	{
		bool result = m_buffer.FirstDiff();
		if (result)
			ScrollToDiff(m_buffer.GetCurrentDiffIndex());
		return result;
	}

	bool LastDiff()
	{
		bool result = m_buffer.LastDiff();
		if (result)
			ScrollToDiff(m_buffer.GetCurrentDiffIndex());
		return result;
	}

	bool NextDiff()
	{
		bool result = m_buffer.NextDiff();
		if (result)
			ScrollToDiff(m_buffer.GetCurrentDiffIndex());
		return result;
	}

	bool PrevDiff()
	{
		bool result = m_buffer.PrevDiff();
		if (result)
			ScrollToDiff(m_buffer.GetCurrentDiffIndex());
		return result;
	}

	bool FirstConflict()
	{
		bool result = m_buffer.FirstConflict();
		if (result)
			ScrollToDiff(m_buffer.GetCurrentDiffIndex());
		return result;
	}

	bool LastConflict()
	{
		bool result = m_buffer.LastConflict();
		if (result)
			ScrollToDiff(m_buffer.GetCurrentDiffIndex());
		return result;
	}

	bool NextConflict()
	{
		bool result = m_buffer.NextConflict();
		if (result)
			ScrollToDiff(m_buffer.GetCurrentDiffIndex());
		return result;
	}

	bool PrevConflict()
	{
		bool result = m_buffer.PrevConflict();
		if (result)
			ScrollToDiff(m_buffer.GetCurrentDiffIndex());
		return result;
	}

	bool SelectDiff(int diffIndex)
	{
		bool result = m_buffer.SelectDiff(diffIndex);
		if (result)
			ScrollToDiff(m_buffer.GetCurrentDiffIndex());
		return result;
	}
	
	int  GetNextDiffIndex() const
	{
		return m_buffer.GetNextDiffIndex();
	}

	int  GetPrevDiffIndex() const
	{
		return m_buffer.GetPrevDiffIndex();
	}

	int  GetNextConflictIndex() const
	{
		return m_buffer.GetNextConflictIndex();
	}

	int  GetPrevConflictIndex() const
	{
		return m_buffer.GetPrevConflictIndex();
	}

	void CopyDiff(int diffIndex, int srcPane, int dstPane)
	{
		m_buffer.CopyDiff(diffIndex, srcPane, dstPane);
		Invalidate();
	}

	void CopyDiffAll(int srcPane, int dstPane)
	{
		m_buffer.CopyDiffAll(srcPane, dstPane);
		Invalidate();
	}

	int CopyDiff3Way(int dstPane)
	{
		int result = m_buffer.CopyDiff3Way(dstPane);
		Invalidate();
		return result;
	}

	bool IsModified(int pane) const
	{
		return m_buffer.IsModified(pane);
	}

	bool IsUndoable() const
	{
		return m_buffer.IsUndoable();
	}

	bool IsRedoable() const
	{
		return m_buffer.IsRedoable();
	}

	bool SelectAll()
	{
		int pane = GetActivePane();
		if (pane < 0)
			return false;
		bool result = m_imgWindow[pane].SelectAll();
		if (result)
			m_imgWindow[pane].Invalidate();
		return result;
	}

	bool Copy()
	{
		int pane = GetActivePane();
		if (pane < 0 || !m_imgWindow[pane].IsRectanlgeSelectionVisible())
			return false;
		Image image;
		RECT rc = m_imgWindow[pane].GetRectangleSelection();
		m_buffer.GetOriginalImage(pane)->copySubImage(image, rc.left, rc.top, rc.right, rc.bottom);
		return !!image.getImage()->copyToClipboard(m_imgWindow[pane].GetHWND());
	}

	bool Cut()
	{
		return true;
	}

	bool Delete()
	{
		int pane = GetActivePane();
		if (pane < 0 || !m_imgWindow[pane].IsRectanlgeSelectionVisible())
			return false;
		RECT rc = m_imgWindow[pane].GetRectangleSelection();
		bool result = m_buffer.DeleteRectangle(pane, rc.left, rc.top, rc.right, rc.bottom);
		if (result)
			Invalidate();
		return result;
	}

	bool Paste()
	{
		return true;
	}

	bool Undo()
	{
		bool result = m_buffer.Undo();
		if (result)
			Invalidate();
		return result;
	}

	bool Redo()
	{
		bool result = m_buffer.Redo();
		if (result)
			Invalidate();
		return result;
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

	bool NewImages(int nImages, int nPages, int width, int height)
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
				m_imgWindow[i].SetWindowRect(rects[i]);
				m_imgWindow[i].SetImage(m_buffer.GetImage(i)->getFipImage());
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
				m_imgWindow[i].SetWindowRect(rects[i]);
				m_imgWindow[i].SetImage(m_buffer.GetImage(i)->getFipImage());
			}

			Event evt;
			evt.eventType = OPEN;
			notify(evt);
		}
		return bSucceeded;
	}

	bool OpenImages(const wchar_t *filename1, const wchar_t *filename2)
	{
		const wchar_t *filenames[] = {filename1, filename2};
		return OpenImages(2, filenames);
	}

	bool OpenImages(const wchar_t *filename1, const wchar_t *filename2, const wchar_t *filename3)
	{
		const wchar_t *filenames[] = {filename1, filename2, filename3};
		return OpenImages(3, filenames);
	}

	bool ReloadImages()
	{
		if (m_nImages == 2)
			return OpenImages(m_buffer.GetFileName(0), m_buffer.GetFileName(1));
		else if (m_nImages == 3)
			return OpenImages(m_buffer.GetFileName(0), m_buffer.GetFileName(1), m_buffer.GetFileName(2));
		return false;
	}

	bool SaveImage(int pane)
	{
		return m_buffer.SaveImage(pane);
	}

	bool SaveImages()
	{
		return m_buffer.SaveImages();
	}

	bool SaveImageAs(int pane, const wchar_t *filename)
	{
		return m_buffer.SaveImageAs(pane, filename);
	}

	bool CloseImages()
	{
		m_buffer.CloseImages();
		if (m_hWnd)
		{
			for (int i = 0; i < m_nImages; ++i)
				m_imgWindow[i].Destroy();
		}
		return true;
	}

	bool SaveDiffImageAs(int pane, const wchar_t *filename)
	{
		return m_buffer.SaveDiffImageAs(pane, filename);
	}

	HWND GetPaneHWND(int pane) const
	{
		if (pane < 0 || pane >= m_nImages || !m_hWnd)
			return NULL;
		return m_imgWindow[pane].GetHWND();
	}

	HWND GetHWND() const
	{
		return m_hWnd;
	}

	int  GetImageWidth(int pane) const
	{
		return m_buffer.GetImageWidth(pane);
	}

	int  GetImageHeight(int pane) const
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

	int  GetImageBitsPerPixel(int pane) const
	{
		return m_buffer.GetImageBitsPerPixel(pane);
	}

	int GetDiffIndexFromPoint(int x, int y) const
	{
		return m_buffer.GetDiffIndexFromPoint(x, y);
	}

	POINT GetImageOffset(int pane) const
	{
		Point<unsigned> pt = m_buffer.GetImageOffset(pane);
		POINT pt2 = {static_cast<long>(pt.x), static_cast<long>(pt.y)};
		return pt2;
	}

	void AddImageOffset(int pane, int dx, int dy)
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

	DRAGGING_MODE GetDraggingMode() const
	{
		return m_draggingMode;
	}

	void SetDraggingMode(DRAGGING_MODE draggingMode)
	{
		m_draggingMode = draggingMode;
	}

	size_t GetMetadata(int pane, char *buf, size_t bufsize) const
	{
		std::map<std::string, std::string> metadata = m_buffer.GetOriginalImage(pane)->getMetadata();
		std::string metadatastr;
		for (auto& it : metadata)
			metadatastr += it.first + ": " + it.second + "\r\n";
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
		wcex.hbrBackground  = (HBRUSH)(COLOR_3DFACE+1);
		wcex.lpszClassName	= L"WinImgMergeWindowClass";
		return RegisterClassExW(&wcex);
	}

	std::vector<RECT> CalcChildImgWindowRect(HWND hWnd, int nImages, bool bHorizontalSplit)
	{
		std::vector<RECT> childrects;
		RECT rcParent;
		GetClientRect(hWnd, &rcParent);
		RECT rc = rcParent;
		if (!bHorizontalSplit)
		{
			int width = (rcParent.left + rcParent.right) / (nImages > 0 ? nImages : 1) - 2;
			rc.left = 0;
			rc.right = rc.left + width;
			for (int i = 0; i < nImages - 1; ++i)
			{
				childrects.push_back(rc);
				rc.left  = rc.right + 2 * 2;
				rc.right = rc.left  + width;
			}
			rc.right = rcParent.right;
			rc.left  = rc.right - width;
			childrects.push_back(rc);
		}
		else
		{
			int height = (rcParent.top + rcParent.bottom) / (nImages > 0 ? nImages : 1) - 2;
			rc.top = 0;
			rc.bottom = rc.top + height;
			for (int i = 0; i < nImages - 1; ++i)
			{
				childrects.push_back(rc);
				rc.top    = rc.bottom + 2 * 2;
				rc.bottom = rc.top    + height;
			}
			rc.bottom = rcParent.bottom;
			rc.top    = rc.bottom - height;
			childrects.push_back(rc);
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

	void OnCreate(HWND hWnd, LPCREATESTRUCT lpCreateStruct)
	{
	}

	void OnSize(UINT nType, int cx, int cy)
	{
		std::vector<RECT> rects = CalcChildImgWindowRect(m_hWnd, m_nImages, m_bHorizontalSplit);
		for (int i = 0; i < m_nImages; ++i)
			m_imgWindow[i].SetWindowRect(rects[i]);
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
		SetCursor(LoadCursor(NULL, m_bHorizontalSplit ? IDC_SIZENS : IDC_SIZEWE));
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
		case WM_TIMER:
			m_buffer.RefreshImages();
			if (m_nImages <= 1)
				break;
			for (int i = 0; i < m_nImages; ++i)
				m_imgWindow[i].Invalidate(false);
			break;
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
		case WM_LBUTTONDOWN:
		{
			pImgWnd->m_bDragging = true;
			pImgWnd->m_ptOrg.x = evt.x;
			pImgWnd->m_ptOrg.y = evt.y;
			pImgWnd->m_ptPrev.x = INT_MIN;
			pImgWnd->m_ptPrev.y = INT_MIN;
			SetCapture(hwnd);
			POINT pt = pImgWnd->GetCursorPos(evt.pane);
			if (pImgWnd->m_draggingMode == DRAGGING_MODE::VERTICAL_WIPE)
			{
				pImgWnd->m_imgWindow[evt.pane].SetRectangleSelection(0, pt.y, pImgWnd->m_buffer.GetImageWidth(evt.pane), pt.y);
				pImgWnd->m_buffer.SetWipeModePosition(CImgDiffBuffer::WIPE_VERTICAL, pt.y);
			}
			else if (pImgWnd->m_draggingMode == DRAGGING_MODE::HORIZONTAL_WIPE)
			{
				pImgWnd->m_imgWindow[evt.pane].SetRectangleSelection(pt.x, 0, pt.x, pImgWnd->m_buffer.GetImageHeight(evt.pane));
				pImgWnd->m_buffer.SetWipeModePosition(CImgDiffBuffer::WIPE_HORIZONTAL, pt.x);
			}
			else if (pImgWnd->m_draggingMode == DRAGGING_MODE::RECTANGLE_SELECT)
			{
				pImgWnd->m_imgWindow[evt.pane].SetRectangleSelection(pt.x, pt.y, pt.x, pt.y);
			}
			pImgWnd->Invalidate();
			break;
		}
		case WM_LBUTTONUP:
			if (pImgWnd->m_bDragging)
			{
				pImgWnd->m_bDragging = false;
				ReleaseCapture();
				if (pImgWnd->m_draggingMode == DRAGGING_MODE::ADJUST_OFFSET)
				{
					POINT ptOffset = pImgWnd->GetImageOffset(evt.pane);
					double zoom = pImgWnd->GetZoom();
					int offsetX = ptOffset.x + static_cast<int>((pImgWnd->m_ptPrev.x - pImgWnd->m_ptOrg.x) / zoom);
					int offsetY = ptOffset.y + static_cast<int>((pImgWnd->m_ptPrev.y - pImgWnd->m_ptOrg.y) / zoom);
					pImgWnd->m_imgWindow[evt.pane].DrawFocusRectangle(offsetX, offsetY, pImgWnd->GetImageWidth(evt.pane), pImgWnd->GetImageHeight(evt.pane));
					pImgWnd->AddImageOffset(evt.pane, static_cast<int>((evt.x - pImgWnd->m_ptOrg.x) / zoom), static_cast<int>((evt.y - pImgWnd->m_ptOrg.y) / zoom));
				}
				else if (pImgWnd->m_draggingMode == DRAGGING_MODE::VERTICAL_WIPE || 
				         pImgWnd->m_draggingMode == DRAGGING_MODE::HORIZONTAL_WIPE)
				{
					pImgWnd->m_buffer.SetWipeMode(CImgDiffBuffer::WIPE_NONE);
					pImgWnd->m_imgWindow[evt.pane].EraseRectangleSelection();
					pImgWnd->Invalidate();
				}
			}
			break;
		case WM_MOUSEMOVE:
			if (pImgWnd->m_bDragging)
			{
				double zoom = pImgWnd->GetZoom();
				if (pImgWnd->m_draggingMode == DRAGGING_MODE::MOVE)
				{
					SCROLLINFO sih{ sizeof SCROLLINFO, SIF_RANGE | SIF_PAGE };
					GetScrollInfo(hwnd, SB_HORZ, &sih);
					if (sih.nMax > static_cast<int>(sih.nPage))
					{
						int posx = GetScrollPos(hwnd, SB_HORZ) + static_cast<int>((pImgWnd->m_ptOrg.x - evt.x) * zoom);
						if (posx < 0)
							posx = 0;
						SendMessage(hwnd, WM_HSCROLL, MAKEWPARAM(SB_THUMBTRACK, posx), 0);
						pImgWnd->m_ptOrg.x = evt.x;
					}

					SCROLLINFO siv{ sizeof SCROLLINFO, SIF_RANGE | SIF_PAGE };
					GetScrollInfo(hwnd, SB_VERT, &siv);
					if (siv.nMax > static_cast<int>(siv.nPage))
					{
						int posy = GetScrollPos(hwnd, SB_VERT) + static_cast<int>((pImgWnd->m_ptOrg.y - evt.y) * zoom);
						if (posy < 0)
							posy = 0;
						SendMessage(hwnd, WM_VSCROLL, MAKEWPARAM(SB_THUMBTRACK, posy), 0);
						pImgWnd->m_ptOrg.y = evt.y;
					}
				}
				else if (pImgWnd->m_draggingMode == DRAGGING_MODE::ADJUST_OFFSET)
				{
					POINT ptOffset = pImgWnd->GetImageOffset(evt.pane);
					if (pImgWnd->m_ptPrev.x != INT_MIN && pImgWnd->m_ptPrev.y != INT_MIN)
					{
						int offsetX = ptOffset.x + static_cast<int>((pImgWnd->m_ptPrev.x - pImgWnd->m_ptOrg.x) / zoom);
						int offsetY = ptOffset.y + static_cast<int>((pImgWnd->m_ptPrev.y - pImgWnd->m_ptOrg.y) / zoom);
						pImgWnd->m_imgWindow[evt.pane].DrawFocusRectangle(offsetX, offsetY, pImgWnd->GetImageWidth(evt.pane), pImgWnd->GetImageHeight(evt.pane));
					}
					int offsetX = ptOffset.x + static_cast<int>((evt.x - pImgWnd->m_ptOrg.x) / zoom);
					int offsetY = ptOffset.y + static_cast<int>((evt.y - pImgWnd->m_ptOrg.y) / zoom);
					pImgWnd->m_imgWindow[evt.pane].DrawFocusRectangle(offsetX, offsetY, pImgWnd->GetImageWidth(evt.pane), pImgWnd->GetImageHeight(evt.pane));
					pImgWnd->m_ptPrev.x = evt.x;
					pImgWnd->m_ptPrev.y = evt.y;
				}
				else if (pImgWnd->m_draggingMode == DRAGGING_MODE::VERTICAL_WIPE)
				{
					POINT pt = pImgWnd->GetCursorPos(evt.pane);
					pImgWnd->m_imgWindow[evt.pane].SetRectangleSelection(0, pt.y, pImgWnd->m_buffer.GetImageWidth(evt.pane), pt.y);
					pImgWnd->m_buffer.SetWipePosition(pt.y);
					pImgWnd->Invalidate();
					SetCursor(LoadCursor(NULL, IDC_SIZENS));
				}
				else if (pImgWnd->m_draggingMode == DRAGGING_MODE::HORIZONTAL_WIPE)
				{
					POINT pt = pImgWnd->GetCursorPos(evt.pane);
					pImgWnd->m_imgWindow[evt.pane].SetRectangleSelection(pt.x, 0, pt.x, pImgWnd->m_buffer.GetImageHeight(evt.pane));
					pImgWnd->m_buffer.SetWipePosition(pt.x);
					pImgWnd->Invalidate();
					SetCursor(LoadCursor(NULL, IDC_SIZEWE));
				}
				else if (pImgWnd->m_draggingMode == DRAGGING_MODE::RECTANGLE_SELECT)
				{
					RECT rcSelect = pImgWnd->m_imgWindow[evt.pane].GetRectangleSelection();
					POINT pt = pImgWnd->GetCursorPos(evt.pane);
					pImgWnd->m_imgWindow[evt.pane].SetRectangleSelection(rcSelect.left, rcSelect.top, pt.x, pt.y);
					pImgWnd->Invalidate();
				}
			}
			break;
		}
		switch (iMsg)
		{
		case WM_LBUTTONDBLCLK:
		{
			POINT pt = pImgWnd->GetCursorPos(i);
			int diffIndex = pImgWnd->GetDiffIndexFromPoint(pt.x, pt.y);
			if (diffIndex >= 0)
				pImgWnd->SelectDiff(diffIndex);
			else
				pImgWnd->SelectDiff(-1);
			break;
		}
		case WM_HSCROLL:
		case WM_VSCROLL:
			if (LOWORD(wParam) == SB_THUMBTRACK)
			{
				SCROLLINFO si{ sizeof SCROLLINFO, SIF_TRACKPOS };
				GetScrollInfo(hwnd, (iMsg == WM_HSCROLL) ? SB_HORZ : SB_VERT, &si);
				wParam |= (si.nTrackPos & 0xff0000) >> 8;
			}
		case WM_MOUSEWHEEL:
			POINT ptLP = pImgWnd->m_imgWindow[i].GetCursorPos();
			POINT ptDP;
			::GetCursorPos(&ptDP);
			RECT rc;
			::GetWindowRect(pImgWnd->m_imgWindow[i].GetHWND(), &rc);
			ptDP.x -= rc.left;
			ptDP.y -= rc.top;
			for (int j = 0; j < pImgWnd->m_nImages; ++j)
			{
				(pImgWnd->m_ChildWndProc[j])(pImgWnd->m_imgWindow[j].GetHWND(), iMsg, wParam, lParam);
				if (GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL)
					pImgWnd->m_imgWindow[j].ScrollTo2(ptLP.x, ptLP.y, ptDP.x, ptDP.y);
			}
			return 0;
		}
		return (pImgWnd->m_ChildWndProc[i])(hwnd, iMsg, wParam, lParam);
	}

	int m_nImages;
	HWND m_hWnd;
	HINSTANCE m_hInstance;
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
	CImgMergeBuffer m_buffer;
	ULONG_PTR m_gdiplusToken;

};
