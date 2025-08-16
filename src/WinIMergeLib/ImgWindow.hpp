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

#include "FreeImagePlus.h"
#include <vector>
#include <algorithm>

class CImgWindow
{
	enum { MARGIN = 16 };
public:
	CImgWindow()
		: m_fip(nullptr)
		, m_hWnd(nullptr)
		, m_hCursor(nullptr)
		, m_nVScrollPos(0)
		, m_nHScrollPos(0)
		, m_zoom(1.0)
		, m_useBackColor(false)
		, m_visibleRectangleSelection(false)
		, m_ptOverlappedImage{}
		, m_ptOverlappedImageCursor{}
		, m_wBar{SB_BOTH}
	{
		memset(&m_backColor, 0xff, sizeof(m_backColor));
	}

	~CImgWindow()
	{
	}

	HWND GetHWND() const
	{
		return m_hWnd;
	}

	bool Create(HINSTANCE hInstance, HWND hWndParent)
	{
		MyRegisterClass(hInstance);
		m_hWnd = CreateWindowExW(0, L"WinImgWindowClass", NULL, WS_CHILD | WS_HSCROLL | WS_VSCROLL | WS_VISIBLE,
			0, 0, 0, 0, hWndParent, NULL, hInstance, this);
		return m_hWnd ? true : false;
	}

	bool Destroy()
	{
		if (m_hWnd)
			DestroyWindow(m_hWnd);
		m_fip = NULL;
		m_hWnd = NULL;
		return true;
	}

	RECT GetWindowRect() const
	{
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

	void SetWindowRect(const RECT& rc)
	{
		MoveWindow(m_hWnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
		if (m_wBar == SB_BOTH)
			::ShowScrollBar(m_hWnd, m_wBar, true);
		else if (m_wBar == SB_HORZ)
		{
			::ShowScrollBar(m_hWnd, SB_HORZ, true);
			::ShowScrollBar(m_hWnd, SB_VERT, false);
		}
		else if (m_wBar == SB_VERT)
		{
			::ShowScrollBar(m_hWnd, SB_HORZ, false);
			::ShowScrollBar(m_hWnd, SB_VERT, true);
		}
	}

	void SetFocus()
	{
		::SetFocus(m_hWnd);
	}

	POINT ConvertDPtoLP(int dx, int dy) const
	{
		POINT lp;
		RECT rc;
		GetClientRect(m_hWnd, &rc);

		if (rc.right - rc.left < m_fip->getWidth() * m_zoom + MARGIN * 2)
			lp.x = static_cast<int>((dx - MARGIN + m_nHScrollPos) / m_zoom);
		else
			lp.x = static_cast<int>((dx - (rc.right / 2 - m_fip->getWidth() / 2 * m_zoom)) / m_zoom);
		if (rc.bottom - rc.top < m_fip->getHeight() * m_zoom + MARGIN * 2)
			lp.y = static_cast<int>((dy - MARGIN + m_nVScrollPos) / m_zoom);
		else
			lp.y = static_cast<int>((dy - (rc.bottom / 2 - m_fip->getHeight() / 2 * m_zoom)) / m_zoom);
		return lp;
	}

	POINT ConvertLPtoDP(int lx, int ly) const
	{
		POINT dp;
		RECT rc;
		GetClientRect(m_hWnd, &rc);

		if (rc.right - rc.left > m_fip->getWidth() * m_zoom + MARGIN * 2)
			dp.x = static_cast<int>(((rc.right - rc.left) - m_fip->getWidth() * m_zoom) / 2);
		else
			dp.x = -m_nHScrollPos + MARGIN; 
		if (rc.bottom - rc.top > m_fip->getHeight() * m_zoom + MARGIN * 2)
			dp.y = static_cast<int>(((rc.bottom - rc.top) - m_fip->getHeight() * m_zoom) / 2); 
		else
			dp.y = -m_nVScrollPos + MARGIN;
		dp.x += static_cast<int>(lx * m_zoom);
		dp.y += static_cast<int>(ly * m_zoom);
		return dp;
	}

	POINT GetCursorPos() const
	{
		POINT dpt;
		::GetCursorPos(&dpt);
		RECT rc;
		::GetWindowRect(m_hWnd, &rc);
		dpt.x -= rc.left;
		dpt.y -= rc.top;
		return ConvertDPtoLP(dpt.x, dpt.y);
	}

	bool IsFocused() const
	{
		return m_hWnd == GetFocus();
	}

	void ScrollTo(int x, int y, bool force = false)
	{
		SCROLLINFO sih{ sizeof SCROLLINFO, SIF_POS | SIF_RANGE | SIF_PAGE | SIF_TRACKPOS };
		SCROLLINFO siv{ sizeof SCROLLINFO, SIF_POS | SIF_RANGE | SIF_PAGE | SIF_TRACKPOS };
		GetScrollInfo(m_hWnd, SB_HORZ, &sih);
		GetScrollInfo(m_hWnd, SB_VERT, &siv);

		RECT rc;
		GetClientRect(m_hWnd, &rc);

		if (rc.right - rc.left < m_fip->getWidth() * m_zoom + MARGIN * 2)
		{
			if (force)
			{
				m_nHScrollPos = static_cast<int>(x * m_zoom + MARGIN - rc.right / 2);
			}
			else
			{
				if (x * m_zoom + MARGIN < m_nHScrollPos || m_nHScrollPos + rc.right < x * m_zoom + MARGIN)
					m_nHScrollPos = static_cast<int>(x * m_zoom + MARGIN - rc.right / 2);
			}
			if (m_nHScrollPos < 0)
				m_nHScrollPos = 0;
			else if (m_nHScrollPos > sih.nMax - static_cast<int>(sih.nPage))
				m_nHScrollPos = sih.nMax - sih.nPage;
		}
		if (rc.bottom - rc.top < m_fip->getHeight() * m_zoom + MARGIN * 2)
		{
			if (force)
			{
				m_nVScrollPos = static_cast<int>(y * m_zoom + MARGIN - rc.bottom / 2);
			}
			else
			{
				if (y * m_zoom + MARGIN < m_nVScrollPos || m_nVScrollPos + rc.bottom < y * m_zoom + MARGIN)
					m_nVScrollPos = static_cast<int>(y * m_zoom + MARGIN - rc.bottom / 2);
			}
			if (m_nVScrollPos < 0)
				m_nVScrollPos = 0;
			else if (m_nVScrollPos > siv.nMax - static_cast<int>(siv.nPage))
				m_nVScrollPos = siv.nMax - siv.nPage;
		}

		CalcScrollBarRange();
		ScrollWindow(m_hWnd, sih.nPos - m_nHScrollPos, siv.nPos - m_nVScrollPos, NULL, NULL);
		InvalidateRect(m_hWnd, NULL, FALSE);
	}

	void ScrollTo2(int lx, int ly, int dx, int dy)
	{
		SCROLLINFO sih{ sizeof SCROLLINFO, SIF_POS | SIF_RANGE | SIF_PAGE | SIF_TRACKPOS };
		SCROLLINFO siv{ sizeof SCROLLINFO, SIF_POS | SIF_RANGE | SIF_PAGE | SIF_TRACKPOS };
		GetScrollInfo(m_hWnd, SB_HORZ, &sih);
		GetScrollInfo(m_hWnd, SB_VERT, &siv);

		RECT rc;
		GetClientRect(m_hWnd, &rc);

		m_nHScrollPos = static_cast<int>(lx * m_zoom + MARGIN - dx);
		if (m_nHScrollPos < 0)
			m_nHScrollPos = 0;
		else if (m_nHScrollPos > sih.nMax - static_cast<int>(sih.nPage))
			m_nHScrollPos = sih.nMax - sih.nPage;
		m_nVScrollPos = static_cast<int>(ly * m_zoom + MARGIN - dy);
		if (m_nVScrollPos < 0)
			m_nVScrollPos = 0;
		else if (m_nVScrollPos > siv.nMax - static_cast<int>(siv.nPage))
			m_nVScrollPos = siv.nMax - siv.nPage;

		CalcScrollBarRange();
		ScrollWindow(m_hWnd, sih.nPos - m_nHScrollPos, siv.nPos - m_nVScrollPos, NULL, NULL);
		InvalidateRect(m_hWnd, NULL, FALSE);
	}

	RGBQUAD GetBackColor() const
	{
		return m_backColor;
	}

	void SetBackColor(RGBQUAD backColor)
	{
		m_backColor = backColor;
		if (m_fip)
		{
			m_fip->setModified(true);
			InvalidateRect(m_hWnd, NULL, TRUE);
		}
	}

	bool GetUseBackColor() const
	{
		return m_useBackColor;
	}

	void SetUseBackColor(bool useBackColor)
	{
		m_useBackColor = useBackColor;
		if (m_fip)
		{
			m_fip->setModified(true);
			InvalidateRect(m_hWnd, NULL, TRUE);
		}
	}

	double GetZoom() const
	{
		return m_zoom;
	}

	void SetZoom(double zoom)
	{
		double oldZoom = m_zoom;
		m_zoom = zoom;
		if (m_zoom < 0.1)
			m_zoom = 0.1;
		m_nVScrollPos = static_cast<int>(m_nVScrollPos / oldZoom * m_zoom);
		m_nHScrollPos = static_cast<int>(m_nHScrollPos / oldZoom * m_zoom);
		if (m_fip)
		{
			RECT rc;
			GetClientRect(m_hWnd, &rc);
			unsigned width  = static_cast<unsigned>(m_fip->getWidth()  * m_zoom) + MARGIN * 2; 
			unsigned height = static_cast<unsigned>(m_fip->getHeight() * m_zoom) + MARGIN * 2; 
			if (m_nHScrollPos > static_cast<int>(width  - rc.right))
				m_nHScrollPos = width  - rc.right;
			if (m_nHScrollPos < 0)
				m_nHScrollPos = 0;
			if (m_nVScrollPos > static_cast<int>(height - rc.bottom))
				m_nVScrollPos = height - rc.bottom;
			if (m_nVScrollPos < 0)
				m_nVScrollPos = 0;
			CalcScrollBarRange();
			InvalidateRect(m_hWnd, NULL, TRUE);
		}
	}

	void Invalidate(bool erase = false)
	{
		InvalidateRect(m_hWnd, NULL, erase);
	}

	void SetImage(fipWinImage *pfip)
	{
		m_fip = pfip;
		m_visibleRectangleSelection = false;
		m_ptSelectionStart = {};
		m_ptSelectionEnd   = {};
		CalcScrollBarRange();
	}

	void SetCursor(HCURSOR hCursor)
	{
		m_hCursor = hCursor;
	}

	POINT GetRectangleSelectionStart() const
	{
		return m_ptSelectionStart;
	}

	bool SetRectangleSelectionStart(int x, int y, bool clamp = true)
	{
		if (!m_fip)
			return false;
		if (clamp)
		{
			m_ptSelectionStart.x = std::clamp(x, 0, static_cast<int>(m_fip->getWidth()));
			m_ptSelectionStart.y = std::clamp(y, 0, static_cast<int>(m_fip->getHeight()));
		}
		else
		{
			m_ptSelectionStart.x = x;
			m_ptSelectionStart.y = y;
		}
		m_visibleRectangleSelection = true;
		return true;
	}

	POINT GetRectangleSelectionEnd() const
	{
		return m_ptSelectionEnd;
	}

	bool SetRectangleSelectionEnd(int x, int y, bool clamp = true)
	{
		if (!m_fip)
			return false;
		if (clamp)
		{
			m_ptSelectionEnd.x = std::clamp(x, 0, static_cast<int>(m_fip->getWidth()));
			m_ptSelectionEnd.y = std::clamp(y, 0, static_cast<int>(m_fip->getHeight()));
		}
		else
		{
			m_ptSelectionEnd.x = x;
			m_ptSelectionEnd.y = y;
		}
		return true;
	}

	bool SetRectangleSelection(int left, int top, int right, int bottom, bool clamp = true)
	{
		SetRectangleSelectionStart(left, top, clamp);
		return SetRectangleSelectionEnd(right, bottom, clamp);
	}

	RECT GetRectangleSelection() const
	{
		int left    = (std::min)(m_ptSelectionStart.x, m_ptSelectionEnd.x);
		int top     = (std::min)(m_ptSelectionStart.y, m_ptSelectionEnd.y);
		int right   = (std::max)(m_ptSelectionStart.x, m_ptSelectionEnd.x);
		int bottom  = (std::max)(m_ptSelectionStart.y, m_ptSelectionEnd.y);
		return { left, top, right, bottom };
	}

	bool IsRectanlgeSelectionVisible() const
	{
		return m_visibleRectangleSelection;
	}

	void DeleteRectangleSelection()
	{
		m_ptSelectionStart = {};
		m_ptSelectionEnd   = {};
		m_visibleRectangleSelection = false;
	}

	const fipWinImage& GetOverlappedImage() const
	{
		return m_fipOverlappedImage;
	}

	void SetOverlappedImage(const fipWinImage& image)
	{
		m_fipOverlappedImage = image;
		m_ptOverlappedImage = {};
		m_ptOverlappedImageCursor = {};
	}

	void StartDraggingOverlappedImage(const fipWinImage& image, const POINT& ptImage, const POINT& ptCursor)
	{
		m_fipOverlappedImage = image;
		m_ptOverlappedImage = ptImage;
		m_ptOverlappedImageCursor = ptCursor;
	}

	void RestartDraggingOverlappedImage(const POINT& ptCursor)
	{
		m_ptOverlappedImageCursor = ptCursor;
	}

	void DragOverlappedImage(const POINT& ptCursor)
	{
		m_ptOverlappedImage.x += ptCursor.x - m_ptOverlappedImageCursor.x;
		m_ptOverlappedImage.y += ptCursor.y - m_ptOverlappedImageCursor.y;
		m_ptOverlappedImageCursor = ptCursor;
	}

	void DeleteOverlappedImage()
	{
		m_fipOverlappedImage.clear();
	}

	POINT GetOverlappedImagePosition() const
	{
		return m_ptOverlappedImage;
	}

	RECT GetOverlappedImageRect() const
	{
		return { m_ptOverlappedImage.x, m_ptOverlappedImage.y,
			static_cast<long>(m_ptOverlappedImage.x + m_fipOverlappedImage.getWidth()),
			static_cast<long>(m_ptOverlappedImage.y + m_fipOverlappedImage.getHeight()) };
	}

	void SetOverlappedImagePosition(const POINT& pt)
	{
		m_ptOverlappedImage = pt;
	}

	void UpdateScrollBars()
	{
		CalcScrollBarRange();
	}

	void SetScrollBar(int wBar)
	{
		m_wBar = wBar;
	}

	bool IsDarkBackgroundEnabled() const
	{
		return s_bDarkBackgroundEnabled;
	}

	void SetDarkBackgroundEnabled(bool enabled)
	{
		s_bDarkBackgroundEnabled = enabled;
		if (m_fip)
			InvalidateRect(m_hWnd, NULL, TRUE);
	}

private:

	ATOM MyRegisterClass(HINSTANCE hInstance)
	{
		WNDCLASSEXW wcex = {0};
		if (!GetClassInfoEx(hInstance, L"WinImgWindowClass", &wcex))
		{
			wcex.cbSize         = sizeof(WNDCLASSEX); 
			wcex.style			= CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
			wcex.lpfnWndProc	= (WNDPROC)WndProc;
			wcex.cbClsExtra		= 0;
			wcex.cbWndExtra		= 0;
			wcex.hInstance		= hInstance;
			wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
			wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW + 1);
			wcex.lpszClassName	= L"WinImgWindowClass";
		}
		return RegisterClassExW(&wcex);
	}

	void OnPaint()
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(m_hWnd, &ps);
		if (m_fip)
		{
			RECT rc;
			GetClientRect(m_hWnd, &rc);
			HDC hdcMem = CreateCompatibleDC(hdc);
			HBITMAP hbmMem = CreateCompatibleBitmap(hdc, rc.right - rc.left, rc.bottom - rc.top);
			HDC hOld = static_cast<HDC>(SelectObject(hdcMem, hbmMem));
			HBRUSH hOldBrush = static_cast<HBRUSH>(SelectObject(hdcMem, static_cast<HGDIOBJ>(CreateSolidBrush(
				s_bDarkBackgroundEnabled ? RGB(40, 40, 60) : RGB(206, 215, 230)))));

			PatBlt(hdcMem, 0, 0, rc.right - rc.left, rc.bottom - rc.top, PATCOPY);

			if (m_fip->isValid())
			{
				POINT pt = ConvertLPtoDP(0, 0);
				RECT rcImg = { pt.x, pt.y, pt.x + static_cast<int>(m_fip->getWidth() * m_zoom), pt.y + static_cast<int>(m_fip->getHeight() * m_zoom) };

				if (rcImg.left <= -32767 / 2 || rcImg.right >= 32767 / 2 || rcImg.top <= -32767 / 2 || rcImg.bottom >= 32767 / 2)
				{
					fipWinImage fipSubImage;
					POINT ptTmpLT = ConvertDPtoLP(0, 0);
					POINT ptTmpRB = ConvertDPtoLP(rc.right + static_cast<int>(1 * m_zoom), static_cast<int>(rc.bottom + 1 * m_zoom));
					POINT ptSubLT = { (ptTmpLT.x >= 0) ? ptTmpLT.x : 0, (ptTmpLT.y >= 0) ? ptTmpLT.y : 0 };
					POINT ptSubRB = {
						ptTmpRB.x < static_cast<int>(m_fip->getWidth()) ? ptTmpRB.x : static_cast<int>(m_fip->getWidth()),
						ptTmpRB.y < static_cast<int>(m_fip->getHeight()) ? ptTmpRB.y : static_cast<int>(m_fip->getHeight())
					};
					POINT ptSubLTDP = ConvertLPtoDP(ptSubLT.x, ptSubLT.y);
					POINT ptSubRBDP = ConvertLPtoDP(ptSubRB.x, ptSubRB.y);
					rcImg = { ptSubLTDP.x, ptSubLTDP.y, ptSubRBDP.x, ptSubRBDP.y };
					m_fip->copySubImage(fipSubImage, ptSubLT.x, ptSubLT.y, ptSubRB.x, ptSubRB.y);
					fipSubImage.drawEx(hdcMem, rcImg, false, m_useBackColor ? &m_backColor : NULL);
				}
				else
				{
					m_fip->drawEx(hdcMem, rcImg, false, m_useBackColor ? &m_backColor : NULL);
				}
			}
			
			if (m_visibleRectangleSelection)
			{
				RECT rcSelectionL = GetRectangleSelection();
				POINT ptLT = ConvertLPtoDP(rcSelectionL.left, rcSelectionL.top);
				POINT ptRB = ConvertLPtoDP(rcSelectionL.right, rcSelectionL.bottom);
				RECT rcSelection = { ptLT.x, ptLT.y, ptRB.x, ptRB.y };
				if (rcSelection.left == rcSelection.right || rcSelection.top == rcSelection.bottom)
				{
					DrawXorBar(hdcMem, rcSelection.left, rcSelection.top,
						rcSelection.right - rcSelection.left + 1, rcSelection.bottom - rcSelection.top + 1);
				}
				else
				{
					DrawXorRectangle(hdcMem, rcSelection.left, rcSelection.top, rcSelection.right - rcSelection.left, rcSelection.bottom - rcSelection.top, 1);
				}
			}

			if (m_fipOverlappedImage.isValid())
			{
				POINT pt = ConvertLPtoDP(m_ptOverlappedImage.x, m_ptOverlappedImage.y);
				RECT rcImg = { pt.x, pt.y, pt.x + static_cast<int>(m_fipOverlappedImage.getWidth() * m_zoom), pt.y + static_cast<int>(m_fipOverlappedImage.getHeight() * m_zoom) };
				m_fipOverlappedImage.draw(hdcMem, rcImg);
				DrawXorRectangle(hdcMem, rcImg.left, rcImg.top, rcImg.right - rcImg.left, rcImg.bottom - rcImg.top, 1);
			}

			BitBlt(hdc, 0, 0, rc.right - rc.left, rc.bottom - rc.top, hdcMem, 0, 0, SRCCOPY);

			DeleteObject(SelectObject(hdcMem, hOldBrush));
			SelectObject(hdcMem, hOld);
			DeleteObject(hbmMem);
			DeleteDC(hdcMem);
		}
		EndPaint(m_hWnd, &ps);
	}

	void OnSize(UINT nType, int cx, int cy)
	{
		CalcScrollBarRange();
	}

	void OnHScroll(UINT nSBCode, UINT nPos)
	{
		SCROLLINFO si{ sizeof SCROLLINFO, SIF_POS | SIF_RANGE | SIF_PAGE | SIF_TRACKPOS };
		GetScrollInfo(m_hWnd, SB_HORZ, &si);
		switch (nSBCode) {
		case SB_LINEUP:
			--m_nHScrollPos;
			break;
		case SB_LINEDOWN:
			++m_nHScrollPos;
			break;
		case SB_PAGEUP:
			m_nHScrollPos -= si.nPage;
			break;
		case SB_PAGEDOWN:
			m_nHScrollPos += si.nPage;
			break;
		case SB_THUMBTRACK:
			m_nHScrollPos = nPos;
			break;
		default: break;
		}
		CalcScrollBarRange();
		ScrollWindow(m_hWnd, si.nPos - m_nHScrollPos, 0, NULL, NULL);
	}

	void OnVScroll(UINT nSBCode, UINT nPos)
	{
		SCROLLINFO si{ sizeof SCROLLINFO, SIF_POS | SIF_RANGE | SIF_PAGE | SIF_TRACKPOS };
		GetScrollInfo(m_hWnd, SB_VERT, &si);
		switch (nSBCode) {
		case SB_LINEUP:
			--m_nVScrollPos;
			break;
		case SB_LINEDOWN:
			++m_nVScrollPos;
			break;
		case SB_PAGEUP:
			m_nVScrollPos -= si.nPage;
			break;
		case SB_PAGEDOWN:
			m_nVScrollPos += si.nPage;
			break;
		case SB_THUMBTRACK:
			m_nVScrollPos = nPos;
			break;
		default: break;
		}
		CalcScrollBarRange();
		ScrollWindow(m_hWnd, 0, si.nPos - m_nVScrollPos, NULL, NULL);
	}

	void OnLButtonDown(UINT nFlags, int x, int y)
	{
		SetFocus();
	}

	void OnRButtonDown(UINT nFlags, int x, int y)
	{
		SetFocus();
	}

	void OnMouseWheel(UINT nFlags, short zDelta)
	{
		if (!(nFlags & MK_CONTROL))
		{ 
			RECT rc;
			GetClientRect(m_hWnd, &rc);
			if (!(nFlags & MK_SHIFT))
			{
				if (rc.bottom - rc.top < m_fip->getHeight() * m_zoom + MARGIN * 2)
				{
					SCROLLINFO si{ sizeof SCROLLINFO, SIF_POS | SIF_RANGE | SIF_PAGE | SIF_TRACKPOS };
					GetScrollInfo(m_hWnd, SB_VERT, &si);
					m_nVScrollPos += - zDelta / (WHEEL_DELTA / 16);
					CalcScrollBarRange();
					ScrollWindow(m_hWnd, 0, si.nPos - m_nVScrollPos, NULL, NULL);
				}
			}
			else
			{
				if (rc.right - rc.left < m_fip->getWidth() * m_zoom + MARGIN * 2)
				{
					SCROLLINFO si{ sizeof SCROLLINFO, SIF_POS | SIF_RANGE | SIF_PAGE | SIF_TRACKPOS };
					GetScrollInfo(m_hWnd, SB_HORZ, &si);
					m_nHScrollPos += - zDelta / (WHEEL_DELTA / 16);
					CalcScrollBarRange();
					ScrollWindow(m_hWnd, si.nPos - m_nHScrollPos, 0, NULL, NULL);
				}
			}
		}
		else
		{
			SetZoom(m_zoom + (zDelta > 0 ? 0.1 : -0.1));
		}
	}

	void OnSetFocus(HWND hwndOld)
	{
		InvalidateRect(m_hWnd, NULL, TRUE);
	}

	void OnKillFocus(HWND hwndNew)
	{
		InvalidateRect(m_hWnd, NULL, TRUE);
	}

	LRESULT OnWndMsg(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
	{
		switch (iMsg)
		{
		case WM_PAINT:
			OnPaint();
			break;
		case WM_ERASEBKGND:
			return TRUE;
		case WM_HSCROLL:
			OnHScroll((UINT)(LOWORD(wParam) & 0xff), (int)(unsigned short)HIWORD(wParam) | ((LOWORD(wParam) & 0xff00) << 8)); // See 'case WM_HSCROLL:' in CImgMergeWindow::ChildWndProc() 
			break;
		case WM_VSCROLL:
			OnVScroll((UINT)(LOWORD(wParam) & 0xff), (int)(unsigned short)HIWORD(wParam) | ((LOWORD(wParam) & 0xff00) << 8)); // See 'case WM_VSCROLL:' in CImgMergeWindow::ChildWndProc() 
			break;
		case WM_LBUTTONDOWN:
			OnLButtonDown((UINT)(wParam), (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam));
			break;
		case WM_RBUTTONDOWN:
			OnRButtonDown((UINT)(wParam), (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam));
			break;
		case WM_MOUSEWHEEL:
			OnMouseWheel(GET_KEYSTATE_WPARAM(wParam), GET_WHEEL_DELTA_WPARAM(wParam));
			break;
		case WM_SETFOCUS:
			OnSetFocus((HWND)wParam);
			break;
		case WM_KILLFOCUS:
			OnKillFocus((HWND)wParam);
			break;
		case WM_COMMAND:
			PostMessage(GetParent(m_hWnd), iMsg, wParam, lParam);
			break;
		case WM_SIZE:
			OnSize((UINT)wParam, LOWORD(lParam), HIWORD(lParam));
			break;
		case WM_SETCURSOR:
			::SetCursor(m_hCursor ? m_hCursor : LoadCursor(nullptr, IDC_ARROW));
			break;
		default:
			return DefWindowProc(hwnd, iMsg, wParam, lParam);
		}
		return 0;
	}

	void CalcScrollBarRange()
	{
		RECT rc;
		GetClientRect(m_hWnd, &rc);
		SCROLLINFO si{ sizeof(SCROLLINFO) };
		if (m_fip)
		{
			unsigned width  = static_cast<unsigned>(m_fip->getWidth()  * m_zoom) + MARGIN * 2;
			unsigned height = static_cast<unsigned>(m_fip->getHeight() * m_zoom) + MARGIN * 2; 

			si.fMask = SIF_POS | SIF_RANGE | SIF_PAGE | SIF_DISABLENOSCROLL;
			si.nMin = 0;
			si.nMax = height;
			si.nPage = rc.bottom;
			si.nPos = m_nVScrollPos;
			SetScrollInfo(m_hWnd, SB_VERT, &si, TRUE);
			m_nVScrollPos = GetScrollPos(m_hWnd, SB_VERT);

			si.nMin = 0;
			si.nMax = width;
			si.nPage = rc.right;
			si.nPos = m_nHScrollPos;
			SetScrollInfo(m_hWnd, SB_HORZ, &si, TRUE);
			m_nHScrollPos = GetScrollPos(m_hWnd, SB_HORZ);
		}
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

	void DrawXorRectangle(HDC hdc, int left, int top, int width, int height, int lineWidth)
	{
		int right = left + width;
		int bottom = top + height;
		DrawXorBar(hdc, left                 , top                   , width    , lineWidth);
		DrawXorBar(hdc, left                 , bottom - lineWidth + 1, width    , lineWidth);
		DrawXorBar(hdc, left                 , top                   , lineWidth, height);
		DrawXorBar(hdc, right - lineWidth + 1, top                   , lineWidth, height);
	}

	static LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
	{
		if (iMsg == WM_NCCREATE)
			SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<LPCREATESTRUCT>(lParam)->lpCreateParams));
		CImgWindow *pImgWnd = reinterpret_cast<CImgWindow *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
		LRESULT lResult = pImgWnd->OnWndMsg(hwnd, iMsg, wParam, lParam);
		return lResult;
	}

	HWND m_hWnd;
	fipWinImage *m_fip;
	fipWinImage m_fipOverlappedImage;
	POINT m_ptOverlappedImage;
	POINT m_ptOverlappedImageCursor;
	int m_nVScrollPos;
	int m_nHScrollPos;
	double m_zoom;
	bool m_useBackColor;
	RGBQUAD m_backColor;
	bool m_visibleRectangleSelection;
	POINT m_ptSelectionStart;
	POINT m_ptSelectionEnd;
	HCURSOR m_hCursor;
	int m_wBar;
	inline static bool s_bDarkBackgroundEnabled = false;
};
