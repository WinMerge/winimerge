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

#include "image.hpp"
#include "ImgConverter.hpp"
#include "Diff.hpp"
#include <string>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <chrono>
#include <cmath>
#include <cassert>

enum OP_TYPE
{
	OP_NONE = 0, OP_1STONLY, OP_2NDONLY, OP_3RDONLY, OP_DIFF, OP_TRIVIAL
};

template<class T> struct Point
{
	Point(): x(0), y(0) {}
	Point(T x, T y): x(x), y(y) {}
	T x, y;
};

template<class T> struct Size
{
	Size(T cx, T cy): cx(cx), cy(cy) {}
	T cx, cy;
};

template<class T> struct Rect
{
	Rect(T left, T top, T right, T bottom): left(left), top(top), right(right), bottom(bottom) {}
	T left, top, right, bottom;
};

template <class T> struct Array2D
{
	Array2D() : m_width(0), m_height(0), m_data(NULL)
	{
	}

	Array2D(size_t width, size_t height) : m_width(width), m_height(height), m_data(new T[width * height])
	{
		memset(m_data, 0, m_width * m_height * sizeof(T));
	}

	Array2D(const Array2D& other) : m_width(other.m_width), m_height(other.m_height), m_data(new T[other.m_width * other.m_height])
	{
		memcpy(m_data, other.m_data, m_width * m_height * sizeof(T));
	}

	Array2D& operator=(const Array2D& other)
	{
		if (this != &other)
		{
			delete[] m_data;
			m_width  = other.m_width;
			m_height = other.m_height;
			m_data = new T[other.m_width * other.m_height];
			memcpy(m_data, other.m_data, m_width * m_height * sizeof(T));
		}
		return *this;
	}

	~Array2D()
	{
		delete[] m_data;
	}

	void resize(size_t width, size_t height)
	{
		delete[] m_data;
		m_data = new T[width * height];
		m_width  = width;
		m_height = height;
		memset(m_data, 0, sizeof(T) * width * height);
	}

	T& operator()(int x, int y)
	{
		return m_data[y * m_width + x];
	}

	const T& operator()(int x, int y) const
	{
		return m_data[y * m_width + x];
	}

	void clear()
	{
		delete[] m_data;
		m_data = NULL;
		m_width = 0;
		m_height = 0;
	}

	size_t height() const
	{
		return m_height;
	}

	size_t width() const
	{
		return m_width;
	}

	size_t m_width, m_height;
	T* m_data;
};

struct DiffInfo
{
	DiffInfo(int op, int x, int y) : op(op), rc(x, y, x + 1, y + 1) {}
	int op;
	Rect<int> rc;
};

struct LineDiffInfo
{
	LineDiffInfo(int s1 = 0, int e1 = 0, int s2 = 0, int e2 = 0, int s3 = 0, int e3 = 0) :
		begin{ s1, s2, s3 }, end{ e1, e2, e3 },
		dbegin(0), dend{ -1, -1, -1 }, dendmax(-1), op(OP_DIFF) {}

	LineDiffInfo(const LineDiffInfo& src) :
		begin{ src.begin[0], src.begin[1], src.begin[2] },
		end{ src.end[0], src.end[1], src.end[2] }, 
		dbegin(src.dbegin),
		dend{ src.dend[0], src.dend[1], src.dend[2] }, 
		dendmax(src.dendmax), op(src.op)
	{}

	int begin[3];
	int end[3];
	int dbegin;
	int dend[3];
	int dendmax;
	int op;
};

struct DiffStat { int d1, d2, d3, detc; };

namespace
{
	int GetColorDistance2(Image::Color c1, Image::Color c2)
	{
		int rdist = Image::valueR(c1) - Image::valueR(c2);
		int gdist = Image::valueG(c1) - Image::valueG(c2);
		int bdist = Image::valueB(c1) - Image::valueB(c2);
		int adist = Image::valueA(c1) - Image::valueA(c2);
		return rdist * rdist + gdist * gdist + bdist * bdist + adist * adist;
	}

	/* diff3 algorithm. It is almost the same as GNU diff3's algorithm */
	template<typename Element, typename Comp02Func>
	std::vector<Element> Make3WayLineDiff(const std::vector<Element>& diff10, const std::vector<Element>& diff12,
		Comp02Func cmpfunc)
	{
		std::vector<Element> diff3;

		size_t diff10count = diff10.size();
		size_t diff12count = diff12.size();

		size_t diff10i = 0;
		size_t diff12i = 0;
		size_t diff3i = 0;

		bool firstDiffBlockIsDiff12;

		Element dr3, dr10, dr12, dr10first, dr10last, dr12first, dr12last;

		int linelast0 = 0;
		int linelast1 = 0;
		int linelast2 = 0;

		for (;;)
		{
			if (diff10i >= diff10count && diff12i >= diff12count)
				break;

			/*
			 * merge overlapped diff blocks
			 * diff10 is diff blocks between file1 and file0.
			 * diff12 is diff blocks between file1 and file2.
			 *
			 *                      diff12
			 *                 diff10            diff3
			 *                 |~~~|             |~~~|
			 * firstDiffBlock  |   |             |   |
			 *                 |   | |~~~|       |   |
			 *                 |___| |   |       |   |
			 *                       |   |   ->  |   |
			 *                 |~~~| |___|       |   |
			 * lastDiffBlock   |   |             |   |
			 *                 |___|             |___|
			 */

			if (diff10i >= diff10count && diff12i < diff12count)
			{
				dr12first = diff12.at(diff12i);
				dr12last = dr12first;
				firstDiffBlockIsDiff12 = true;
			}
			else if (diff10i < diff10count && diff12i >= diff12count)
			{
				dr10first = diff10.at(diff10i);
				dr10last = dr10first;
				firstDiffBlockIsDiff12 = false;
			}
			else
			{
				dr10first = diff10.at(diff10i);
				dr12first = diff12.at(diff12i);
				dr10last = dr10first;
				dr12last = dr12first;
				if (dr12first.begin[0] <= dr10first.begin[0])
					firstDiffBlockIsDiff12 = true;
				else
					firstDiffBlockIsDiff12 = false;
			}
			bool lastDiffBlockIsDiff12 = firstDiffBlockIsDiff12;

			size_t diff10itmp = diff10i;
			size_t diff12itmp = diff12i;
			for (;;)
			{
				if (diff10itmp >= diff10count || diff12itmp >= diff12count)
					break;

				dr10 = diff10.at(diff10itmp);
				dr12 = diff12.at(diff12itmp);

				if (dr10.end[0] == dr12.end[0])
				{
					diff10itmp++;
					lastDiffBlockIsDiff12 = true;

					dr10last = dr10;
					dr12last = dr12;
					break;
				}

				if (lastDiffBlockIsDiff12)
				{
					if ((std::max)(dr12.begin[0], dr12.end[0]) < dr10.begin[0])
						break;
				}
				else
				{
					if ((std::max)(dr10.begin[0], dr10.end[0]) < dr12.begin[0])
						break;
				}

				if (dr12.end[0] > dr10.end[0])
				{
					diff10itmp++;
					lastDiffBlockIsDiff12 = true;
				}
				else
				{
					diff12itmp++;
					lastDiffBlockIsDiff12 = false;
				}

				dr10last = dr10;
				dr12last = dr12;
			}

			if (lastDiffBlockIsDiff12)
				diff12itmp++;
			else
				diff10itmp++;

			if (firstDiffBlockIsDiff12)
			{
				dr3.begin[1] = dr12first.begin[0];
				dr3.begin[2] = dr12first.begin[1];
				if (diff10itmp == diff10i)
					dr3.begin[0] = dr3.begin[1] - linelast1 + linelast0;
				else
					dr3.begin[0] = dr3.begin[1] - dr10first.begin[0] + dr10first.begin[1];
			}
			else
			{
				dr3.begin[0] = dr10first.begin[1];
				dr3.begin[1] = dr10first.begin[0];
				if (diff12itmp == diff12i)
					dr3.begin[2] = dr3.begin[1] - linelast1 + linelast2;
				else
					dr3.begin[2] = dr3.begin[1] - dr12first.begin[0] + dr12first.begin[1];
			}

			if (lastDiffBlockIsDiff12)
			{
				dr3.end[1] = dr12last.end[0];
				dr3.end[2] = dr12last.end[1];
				if (diff10itmp == diff10i)
					dr3.end[0] = dr3.end[1] - linelast1 + linelast0;
				else
					dr3.end[0] = dr3.end[1] - dr10last.end[0] + dr10last.end[1];
			}
			else
			{
				dr3.end[0] = dr10last.end[1];
				dr3.end[1] = dr10last.end[0];
				if (diff12itmp == diff12i)
					dr3.end[2] = dr3.end[1] - linelast1 + linelast2;
				else
					dr3.end[2] = dr3.end[1] - dr12last.end[0] + dr12last.end[1];
			}

			linelast0 = dr3.end[0] + 1;
			linelast1 = dr3.end[1] + 1;
			linelast2 = dr3.end[2] + 1;

			if (diff10i == diff10itmp)
				dr3.op = OP_3RDONLY;
			else if (diff12i == diff12itmp)
				dr3.op = OP_1STONLY;
			else
			{
				if (!cmpfunc(dr3))
					dr3.op = OP_DIFF;
				else
					dr3.op = OP_2NDONLY;
			}

			diff3.push_back(dr3);

			diff3i++;
			diff10i = diff10itmp;
			diff12i = diff12itmp;
		}

		for (size_t i = 0; i < diff3i; i++)
		{
			Element& dr3r = diff3.at(i);
			if (i < diff3i - 1)
			{
				Element& dr3next = diff3.at(i + 1);
				for (int j = 0; j < 3; j++)
				{
					if (dr3r.end[j] >= dr3next.begin[j])
						dr3r.end[j] = dr3next.begin[j] - 1;
				}
			}
		}

		return diff3;
	}

	bool alineEquals(const unsigned char* scanline1, unsigned width1,
		const unsigned char* scanline2, unsigned width2, double colorDistanceThreshold)
	{
		if (width1 != width2)
			return false;
		if (colorDistanceThreshold > 0)
		{
			for (unsigned x = 0; x < width1; ++x)
			{
				int bdist = scanline1[x * 4 + 0] - scanline2[x * 4 + 0];
				int gdist = scanline1[x * 4 + 1] - scanline2[x * 4 + 1];
				int rdist = scanline1[x * 4 + 2] - scanline2[x * 4 + 2];
				int adist = scanline1[x * 4 + 3] - scanline2[x * 4 + 3];
				int colorDistance2 = rdist * rdist + gdist * gdist + bdist * bdist + adist * adist;
				if (colorDistance2 > colorDistanceThreshold * colorDistanceThreshold)
					return false;
			}
			return true;
		}
		else
		{
			return (memcmp(scanline1, scanline2, width1 * 4) == 0);
		}
	}

	bool alineEquals(const Image& img1, const Image& img2,
		unsigned y1, unsigned y2, double colorDistanceThreshold)
	{
		return alineEquals(img1.scanLine(y1), img1.width(), img2.scanLine(y2), img2.width(), colorDistanceThreshold);
	}
}

class DataForDiff
{
public:
	DataForDiff(const Image& img, double colorDistanceThreshold)
		: m_colorDistanceThreshold(colorDistanceThreshold)
		, m_recsize(img.width() * 4)
		, m_recnum(img.height())
	{
		m_data.resize(m_recsize * m_recnum);
		for (unsigned i = 0; i < m_recnum; i++)
			memcpy(m_data.data() + i * m_recsize, img.scanLine(i), m_recsize);
	}
	unsigned size() const { return m_recsize * m_recnum; }
	const char* data() const { return m_data.data(); }
	const char* next(const char* scanline) const
	{
		return scanline + m_recsize;
	}
	bool equals(const char* scanline1, unsigned size1,
		const char* scanline2, unsigned size2) const
	{
		return alineEquals(reinterpret_cast<const unsigned char *>(scanline1), size1 / 4, reinterpret_cast<const unsigned char*>(scanline2), size2 / 4, m_colorDistanceThreshold);
	}
	unsigned long hash(const char* scanline) const
	{
		unsigned long ha = 5381;
		const char* begin = scanline;
		const char* end = begin + m_recsize;

		if (m_colorDistanceThreshold > 0.0)
		{
			int w = static_cast<int>(sqrt((m_colorDistanceThreshold * m_colorDistanceThreshold) / 3.0)) * 2;
			if (w == 0)
				w = 1;
			for (const auto* ptr = begin; ptr < end; ptr++)
			{
				ha += (ha << 5);
				ha ^= ((*ptr / w) * w) & 0xFF;
			}
		}
		else
		{
			for (const auto* ptr = begin; ptr < end; ptr++)
			{
				ha += (ha << 5);
				ha ^= *ptr & 0xFF;
			}
		}
		return ha;
	}

private:
	unsigned m_recnum;
	unsigned m_recsize;
	std::vector<char> m_data;
	double m_colorDistanceThreshold;
};

class CImgDiffBuffer
{
	friend class TemporaryTransformation;
	typedef Array2D<int> DiffBlocks;

public:
	enum INSERTION_DELETION_DETECTION_MODE {
		INSERTION_DELETION_DETECTION_NONE = 0, INSERTION_DELETION_DETECTION_VERTICAL, INSERTION_DELETION_DETECTION_HORIZONTAL
	};
	enum OVERLAY_MODE {
		OVERLAY_NONE = 0, OVERLAY_XOR, OVERLAY_ALPHABLEND, OVERLAY_ALPHABLEND_ANIM
	};
	enum WIPE_MODE {
		WIPE_NONE = 0, WIPE_VERTICAL, WIPE_HORIZONTAL
	};
	enum DIFF_ALGORITHM {
		MYERS_DIFF, MINIMAL_DIFF, PATIENCE_DIFF, HISTOGRAM_DIFF, NONE_DIFF
	};
	
	enum { BLINK_TIME = 800 };
	enum { OVERLAY_ALPHABLEND_ANIM_TIME = 1000 };

	CImgDiffBuffer() : 
		  m_nImages(0)
		, m_showDifferences(true)
		, m_blinkDifferences(false)
		, m_vectorImageZoomRatio(1.0f)
		, m_insertionDeletionDetectionMode(INSERTION_DELETION_DETECTION_NONE)
		, m_overlayMode(OVERLAY_NONE)
		, m_overlayAlpha(0.3)
		, m_wipeMode(WIPE_NONE)
		, m_wipePosition(0)
		, m_diffBlockSize(8)
		, m_selDiffColor(Image::Rgb(0xff, 0x40, 0x40))
		, m_selDiffDeletedColor(Image::Rgb(0xf0, 0xc0, 0xc0))
		, m_diffColor(Image::Rgb(0xff, 0xff, 0x40))
		, m_diffDeletedColor(Image::Rgb(0xc0, 0xc0, 0xc0))
		, m_diffColorAlpha(0.7)
		, m_colorDistanceThreshold(0.0)
		, m_currentDiffIndex(-1)
		, m_diffCount(0)
		, m_angle{}
		, m_horizontalFlip{}
		, m_verticalFlip{}
		, m_temporarilyTransformed(false)
		, m_diffAlgorithm(MYERS_DIFF)
	{
		for (int i = 0; i < 3; ++i)
			m_currentPage[i] = 0;
	}

	virtual ~CImgDiffBuffer()
	{
		CloseImages();
	}

	const wchar_t *GetFileName(int pane)
	{
		if (pane < 0 || pane >= m_nImages)
			return NULL;
		return m_filename[pane].c_str();
	}

	int GetPaneCount() const
	{
		return m_nImages;
	}

	Image::Color GetPixelColor(int pane, int x, int y) const
	{
		return m_imgPreprocessed[pane].pixel(x - m_offset[pane].x, y - m_offset[pane].y);
	}

	double GetColorDistance(int pane1, int pane2, int x, int y) const
	{
		return std::sqrt(static_cast<double>(
			::GetColorDistance2(GetPixelColor(pane1, x, y), GetPixelColor(pane2, x, y)) ));
	}

	Image::Color GetDiffColor() const
	{
		return m_diffColor;
	}

	void SetDiffColor(Image::Color clrDiffColor)
	{
		if (memcmp(&m_diffColor, &clrDiffColor, sizeof(m_diffColor)) == 0)
			return;
		m_diffColor = clrDiffColor;
		RefreshImages();
	}

	Image::Color GetDiffDeletedColor() const
	{
		return m_diffDeletedColor;
	}

	void SetDiffDeletedColor(Image::Color clrDiffDeletedColor)
	{
		if (memcmp(&m_diffDeletedColor, &clrDiffDeletedColor, sizeof(m_diffDeletedColor)) == 0)
			return;
		m_diffDeletedColor = clrDiffDeletedColor;
		RefreshImages();
	}

	Image::Color GetSelDiffColor() const
	{
		return m_selDiffColor;
	}

	void SetSelDiffColor(Image::Color clrSelDiffColor)
	{
		if (memcmp(&m_selDiffColor, &clrSelDiffColor, sizeof(m_selDiffColor)) == 0)
			return;
		m_selDiffColor = clrSelDiffColor;
		RefreshImages();
	}

	Image::Color GetSelDiffDeletedColor() const
	{
		return m_selDiffDeletedColor;
	}

	void SetSelDiffDeletedColor(Image::Color clrSelDiffDeletedColor)
	{
		if (memcmp(&m_selDiffDeletedColor, &clrSelDiffDeletedColor, sizeof(m_selDiffDeletedColor)) == 0)
			return;
		m_selDiffDeletedColor = clrSelDiffDeletedColor;
		RefreshImages();
	}

	double GetDiffColorAlpha() const
	{
		return m_diffColorAlpha;
	}

	void SetDiffColorAlpha(double diffColorAlpha)
	{
		if (m_diffColorAlpha == diffColorAlpha)
			return;
		m_diffColorAlpha = diffColorAlpha;
		RefreshImages();
	}

	int  GetCurrentPage(int pane) const
	{
		if (pane < 0 || pane >= m_nImages)
			return -1;
		return m_currentPage[pane];
	}

	void SetCurrentPage(int pane, int page)
	{
		if (page >= 0 && page < GetPageCount(pane))
		{
			if (m_currentPage[pane] != page &&
			    (m_imgOrigMultiPage[pane].isValid() || m_imgConverter[pane].isValid()))
			{
				m_currentPage[pane] = page;
				ChangePage(pane, page);
				CompareImages();
			}
		}
	}

	void SetCurrentPageAll(int page)
	{
		bool frecompare = false;
		for (int pane = 0; pane < m_nImages; ++pane)
		{
			if (page >= 0 && page < GetPageCount(pane))
			{
				if (m_currentPage[pane] != page &&
				    (m_imgOrigMultiPage[pane].isValid() || m_imgConverter[pane].isValid()))
				{
					m_currentPage[pane] = page;
					ChangePage(pane, page);
					frecompare = true;
				}
			}
		}
		if (frecompare)
			CompareImages();
	}

	int  GetCurrentMaxPage() const
	{
		int maxpage = 0;
		for (int i = 0; i < m_nImages; ++i)
		{
			int page = GetCurrentPage(i);
			maxpage = maxpage < page ? page : maxpage;
		}
		return maxpage;
	}

	int  GetPageCount(int pane) const
	{
		if (pane < 0 || pane >= m_nImages)
			return -1;
		if (m_imgOrigMultiPage[pane].isValid())
			return m_imgOrigMultiPage[pane].getPageCount();
		if (m_imgConverter[pane].isValid())
			return m_imgConverter[pane].getPageCount();
		else
			return 1;
	}

	int  GetMaxPageCount() const
	{
		int maxpage = 0;
		for (int i = 0; i < m_nImages; ++i)
		{
			int page = GetPageCount(i);
			maxpage = page > maxpage ? page : maxpage;
		}
		return maxpage;
	}

	double GetColorDistanceThreshold() const
	{
		return m_colorDistanceThreshold;
	}

	void SetColorDistanceThreshold(double threshold)
	{
		if (m_colorDistanceThreshold == threshold)
			return;
		m_colorDistanceThreshold = threshold;
		CompareImages();
	}

	int  GetDiffBlockSize() const
	{
		return m_diffBlockSize;
	}
	
	void SetDiffBlockSize(int blockSize)
	{
		if (m_diffBlockSize == blockSize)
			return;
		m_diffBlockSize = blockSize;
		CompareImages();
	}

	INSERTION_DELETION_DETECTION_MODE GetInsertionDeletionDetectionMode() const
	{
		return m_insertionDeletionDetectionMode;
	}

	void SetInsertionDeletionDetectionMode(INSERTION_DELETION_DETECTION_MODE insertionDeletionDetectionMode)
	{
		if (m_insertionDeletionDetectionMode == insertionDeletionDetectionMode)
			return;
		m_insertionDeletionDetectionMode = insertionDeletionDetectionMode;
		CompareImages();
	}

	OVERLAY_MODE GetOverlayMode() const
	{
		return m_overlayMode;
	}

	void SetOverlayMode(OVERLAY_MODE overlayMode)
	{
		if (m_overlayMode == overlayMode)
			return;
		m_overlayMode = overlayMode;
		RefreshImages();
	}

	double GetOverlayAlpha() const
	{
		return m_overlayAlpha;
	}

	void SetOverlayAlpha(double overlayAlpha)
	{
		if (m_overlayAlpha == overlayAlpha)
			return;
		m_overlayAlpha = overlayAlpha;
		RefreshImages();
	}

	WIPE_MODE GetWipeMode() const
	{
		return m_wipeMode;
	}

	void SetWipeMode(WIPE_MODE wipeMode)
	{
		if (m_wipeMode == wipeMode)
			return;
		m_wipeMode = wipeMode;
		RefreshImages();
	}

	int GetWipePosition() const
	{
		return m_wipePosition;
	}

	void SetWipePosition(int pos)
	{
		if (m_wipePosition == pos)
			return;
		m_wipePosition = pos;
		RefreshImages();
	}

	void SetWipeModePosition(WIPE_MODE wipeMode, int pos)
	{
		if (m_wipeMode == wipeMode && m_wipePosition == pos)
			return;
		m_wipeMode = wipeMode;
		m_wipePosition = pos;
		RefreshImages();
	}

	bool GetShowDifferences() const
	{
		return m_showDifferences;
	}

	void SetShowDifferences(bool visible)
	{
		if (m_showDifferences == visible)
			return;
		m_showDifferences = visible;
		CompareImages();
	}

	bool GetBlinkDifferences() const
	{
		return m_blinkDifferences;
	}

	void SetBlinkDifferences(bool blink)
	{
		if (m_blinkDifferences == blink)
			return;
		m_blinkDifferences = blink;
		RefreshImages();
	}

	float GetVectorImageZoomRatio() const
	{
		return m_vectorImageZoomRatio;
	}

	void SetVectorImageZoomRatio(float zoom)
	{
		if (m_vectorImageZoomRatio == zoom)
			return;
		m_vectorImageZoomRatio = zoom;
		for (int pane = 0; pane < m_nImages; ++pane)
		{
			if (m_imgConverter[pane].isValid())
				ChangePage(pane, m_currentPage[pane]);
		}
		CompareImages();
	}

	float GetRotation(int pane) const
	{
		if (pane < 0 || pane >= m_nImages)
			return 0.f;
		return m_angle[pane];
	}

	void SetRotation(int pane, float angle)
	{
		if (pane < 0 || pane >= m_nImages)
			return;
		if (angle >= 360.f)
			angle = angle - 360.f * (static_cast<int>(angle) / 360);
		else if (angle < 0.f)
			angle = angle + 360.f * (1 + (static_cast<int>(-angle) / 360));
		if (m_angle[pane] == angle)
			return;
		m_angle[pane] = angle;
		CompareImages();
	}

	bool GetHorizontalFlip(int pane) const
	{
		if (pane < 0 || pane >= m_nImages)
			return false;
		return m_horizontalFlip[pane];
	}

	void SetHorizontalFlip(int pane, bool flip)
	{
		if (pane < 0 || pane >= m_nImages)
			return;
		if (m_horizontalFlip[pane] == flip)
			return;
		m_horizontalFlip[pane] = flip;
		CompareImages();
	}

	bool GetVerticalFlip(int pane ) const
	{
		if (pane < 0 || pane >= m_nImages)
			return false;
		return m_verticalFlip[pane];
	}

	void SetVerticalFlip(int pane, bool flip)
	{
		if (pane < 0 || pane >= m_nImages)
			return;
		if (m_verticalFlip[pane] == flip)
			return;
		m_verticalFlip[pane] = flip;
		CompareImages();
	}

	DIFF_ALGORITHM GetDiffAlgorithm() const
	{
		return m_diffAlgorithm;
	}

	void SetDiffAlgorithm(DIFF_ALGORITHM diffAlgorithm)
	{
		if (m_diffAlgorithm == diffAlgorithm)
			return;
		m_diffAlgorithm = diffAlgorithm;
		CompareImages();
	}

	const DiffInfo *GetDiffInfo(int diffIndex) const
	{
		if (diffIndex < 0 || diffIndex >= m_diffCount)
			return NULL;
		return &m_diffInfos[diffIndex];
	}

	int  GetDiffCount() const
	{
		return m_diffCount;
	}

	int  GetConflictCount() const
	{
		int conflictCount = 0;
		for (int i = 0; i < m_diffCount; ++i)
			if (m_diffInfos[i].op == OP_DIFF)
				++conflictCount;
		return conflictCount;
	}

	int  GetCurrentDiffIndex() const
	{
		return m_currentDiffIndex;
	}

	bool FirstDiff()
	{
		int oldDiffIndex = m_currentDiffIndex;
		if (m_diffCount == 0)
			m_currentDiffIndex = -1;
		else
			m_currentDiffIndex = 0;
		if (oldDiffIndex == m_currentDiffIndex)
			return false;
		RefreshImages();
		return true;
	}

	bool LastDiff()
	{
		int oldDiffIndex = m_currentDiffIndex;
		m_currentDiffIndex = m_diffCount - 1;
		if (oldDiffIndex == m_currentDiffIndex)
			return false;
		RefreshImages();
		return true;
	}

	bool NextDiff()
	{
		int oldDiffIndex = m_currentDiffIndex;
		++m_currentDiffIndex;
		if (m_currentDiffIndex >= m_diffCount)
			m_currentDiffIndex = m_diffCount - 1;
		if (oldDiffIndex == m_currentDiffIndex)
			return false;
		RefreshImages();
		return true;
	}

	bool PrevDiff()
	{
		int oldDiffIndex = m_currentDiffIndex;
		if (m_diffCount == 0)
			m_currentDiffIndex = -1;
		else
		{
			--m_currentDiffIndex;
			if (m_currentDiffIndex < 0)
				m_currentDiffIndex = 0;
		}
		if (oldDiffIndex == m_currentDiffIndex)
			return false;
		RefreshImages();
		return true;
	}

	bool FirstConflict()
	{
		int oldDiffIndex = m_currentDiffIndex;
		for (size_t i = 0; i < m_diffInfos.size(); ++i)
			if (m_diffInfos[i].op == OP_DIFF)
				m_currentDiffIndex = static_cast<int>(i);
		if (oldDiffIndex == m_currentDiffIndex)
			return false;
		RefreshImages();
		return true;
	}

	bool LastConflict()
	{
		int oldDiffIndex = m_currentDiffIndex;
		for (int i = static_cast<int>(m_diffInfos.size() - 1); i >= 0; --i)
		{
			if (m_diffInfos[i].op == OP_DIFF)
			{
				m_currentDiffIndex = i;
				break;
			}
		}
		if (oldDiffIndex == m_currentDiffIndex)
			return false;
		RefreshImages();
		return true;
	}

	bool NextConflict()
	{
		int oldDiffIndex = m_currentDiffIndex;
		for (size_t i = m_currentDiffIndex + 1; i < m_diffInfos.size(); ++i)
		{
			if (m_diffInfos[i].op == OP_DIFF)
			{
				m_currentDiffIndex = static_cast<int>(i);
				break;
			}
		}
		if (oldDiffIndex == m_currentDiffIndex)
			return false;
		RefreshImages();
		return true;
	}

	bool PrevConflict()
	{
		int oldDiffIndex = m_currentDiffIndex;
		for (int i = m_currentDiffIndex - 1; i >= 0; --i)
		{
			if (m_diffInfos[i].op == OP_DIFF)
			{
				m_currentDiffIndex = i;
				break;
			}
		}
		if (oldDiffIndex == m_currentDiffIndex)
			return false;
		RefreshImages();
		return true;
	}

	bool SelectDiff(int diffIndex)
	{
		if (diffIndex == m_currentDiffIndex || diffIndex < -1 || diffIndex >= m_diffCount)
			return false;
		m_currentDiffIndex = diffIndex;
		RefreshImages();
		return true;
	}
	
	int  GetNextDiffIndex() const
	{
		if (m_diffCount == 0 || m_currentDiffIndex >= m_diffCount - 1)
			return -1;
		return m_currentDiffIndex + 1;
	}

	int  GetPrevDiffIndex() const
	{
		if (m_diffCount == 0 || m_currentDiffIndex <= 0)
			return -1;
		return m_currentDiffIndex - 1;
	}

	int  GetNextConflictIndex() const
	{
		for (size_t i = m_currentDiffIndex + 1; i < m_diffInfos.size(); ++i)
			if (m_diffInfos[i].op == OP_DIFF)
				return static_cast<int>(i);
		return -1;
	}

	int  GetPrevConflictIndex() const
	{
		for (int i = static_cast<int>(m_currentDiffIndex - 1); i >= 0; --i)
			if (m_diffInfos[i].op == OP_DIFF)
				return i;
		return -1;
	}

	void CompareImages()
	{
		if (m_nImages <= 1)
			return;

		PreprocessImages();

		InitializeDiff();
		if (m_nImages == 2)
		{
			CompareImages2(0, 1, m_diff);
			m_diffCount = MarkDiffIndex(m_diff);
		}
		else if (m_nImages == 3)
		{
			CompareImages2(0, 1, m_diff01);
			CompareImages2(2, 1, m_diff21);
			CompareImages2(0, 2, m_diff02);
			Make3WayDiff(m_diff01, m_diff21, m_diff);
			m_diffCount = MarkDiffIndex3way(m_diff01, m_diff21, m_diff02, m_diff);
		}
		if (m_currentDiffIndex >= m_diffCount)
			m_currentDiffIndex = m_diffCount - 1;
		RefreshImages();
	}

	void RefreshImages()
	{
		if (m_nImages <= 1)
			return;
		InitializeDiffImages();
		for (int i = 0; i < m_nImages; ++i)
			CopyPreprocessedImageToDiffImage(i);
		void (CImgDiffBuffer::*func)(int src, int dst) = NULL;
		if (m_overlayMode == OVERLAY_ALPHABLEND || m_overlayMode == OVERLAY_ALPHABLEND_ANIM)
			func = &CImgDiffBuffer::AlphaBlendImages2;
		else if (m_overlayMode == OVERLAY_XOR)
			func = &CImgDiffBuffer::XorImages2;
		if (func)
		{
			if (m_nImages == 2)
			{
				(this->*func)(1, 0);
				(this->*func)(0, 1);
			}
			else if (m_nImages == 3)
			{
				(this->*func)(1, 0);
				(this->*func)(0, 1);
				(this->*func)(2, 1);
				(this->*func)(1, 2);
			}
		}
		if (m_wipeMode != WIPE_NONE)
		{
			WipeEffect();
		}
		if (m_showDifferences)
		{
			bool showDiff = true;
			if (m_blinkDifferences)
			{
				auto now = std::chrono::system_clock::now();
				auto tse = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
				if ((tse.count() % BLINK_TIME) < BLINK_TIME / 2)
				{
					showDiff = false;
				}

			}
			if (showDiff)
			{
				for (int i = 0; i < m_nImages; ++i)
					MarkDiff(i, m_diff);
			}
		}
	}

	bool OpenImages(int nImages, const wchar_t * const filename[3])
	{
		CloseImages();
		m_nImages = nImages;
		for (int i = 0; i < nImages; ++i)
			m_filename[i] = filename[i];
		return LoadImages();
	}

	virtual bool CloseImages()
	{
		for (int i = 0; i < m_nImages; ++i)
		{
			m_imgConverter[i].close();
			m_imgOrigMultiPage[i].close();
			m_imgOrig[i].clear();
			m_imgOrig32[i].clear();
			m_imgPreprocessed[i].clear();
			m_offset[i].x = 0;
			m_offset[i].y = 0;
		}
		m_nImages = 0;
		return true;
	}

	bool SaveDiffImageAs(int pane, const wchar_t *filename)
	{
		if (pane < 0 || pane >= m_nImages)
			return false;
		return !!m_imgDiff[pane].save(filename);
	}

	int  GetImageWidth(int pane) const
	{
		if (pane < 0 || pane >= m_nImages)
			return -1;
		if (m_angle[pane] == 0.f || m_angle[pane] == 180.f)
			return m_imgOrig32[pane].width();
		else
			return m_imgOrig32[pane].height();
	}

	int  GetImageHeight(int pane) const
	{
		if (pane < 0 || pane >= m_nImages)
			return -1;
		if (m_angle[pane] == 0.f || m_angle[pane] == 180.f)
			return m_imgOrig32[pane].height();
		else
			return m_imgOrig32[pane].width();
	}

	int  GetPreprocessedImageWidth(int pane) const
	{
		if (pane < 0 || pane >= m_nImages)
			return -1;
		return m_imgPreprocessed[pane].width();
	}

	int  GetPreprocessedImageHeight(int pane) const
	{
		if (pane < 0 || pane >= m_nImages)
			return -1;
		return m_imgPreprocessed[pane].height();
	}

	int  GetDiffImageWidth() const
	{
		if (m_nImages <= 0)
			return -1;
		return m_imgDiff[0].width();
	}

	int  GetDiffImageHeight() const
	{
		if (m_nImages <= 0)
			return -1;
		return m_imgDiff[0].height();
	}

	int  GetImageBitsPerPixel(int pane) const
	{
		if (pane < 0 || pane >= m_nImages)
			return -1;
		return m_imgOrig[pane].depth();
	}

	int GetDiffIndexFromPoint(int x, int y) const
	{
		if (x > 0 && y > 0 && 
			x < static_cast<int>(m_imgDiff[0].width()) &&
			y < static_cast<int>(m_imgDiff[0].height()))
		{
			return m_diff(x / m_diffBlockSize, y / m_diffBlockSize) - 1;
		}
		return -1;
	}

	Image *GetImage(int pane)
	{
		if (pane < 0 || pane >= m_nImages)
			return NULL;
		return &m_imgDiff[pane];
	}

	const Image *GetImage(int pane) const
	{
		if (pane < 0 || pane >= m_nImages)
			return NULL;
		return &m_imgDiff[pane];
	}

	const Image *GetPreprocessedImage(int pane) const
	{
		if (pane < 0 || pane >= m_nImages)
			return NULL;
		return &m_imgPreprocessed[pane];
	}

	const Image *GetOriginalImage(int pane) const
	{
		if (pane < 0 || pane >= m_nImages)
			return NULL;
		return &m_imgOrig[pane];
	}

	Image *GetDiffMapImage(unsigned w, unsigned h)
	{
		m_imgDiffMap.clear();
		m_imgDiffMap.setSize(w, h);
		if (m_nImages == 0)
			return &m_imgDiffMap;
		double diffMapBlockSizeW = static_cast<double>(m_diffBlockSize) * w / m_imgDiff[0].width();
		double diffMapBlockSizeH = static_cast<double>(m_diffBlockSize) * h / m_imgDiff[0].height();
		for (unsigned by = 0; by < m_diff.height(); ++by)
		{
			for (unsigned bx = 0; bx < m_diff.width(); ++bx)
			{
				int diffIndex = m_diff(bx, by);
				if (diffIndex != 0)
				{
					Image::Color color = (diffIndex - 1 == m_currentDiffIndex) ? m_selDiffColor : m_diffColor;
					unsigned bsy = static_cast<unsigned>(diffMapBlockSizeH + 1);
					unsigned y = static_cast<unsigned>(by * diffMapBlockSizeH);
					if (y + bsy - 1 >= h)
						bsy = h - y;
					for (unsigned i = 0; i < bsy; ++i)
					{
						unsigned y = static_cast<unsigned>(by * diffMapBlockSizeH + i);
						unsigned char *scanline = m_imgDiffMap.scanLine(y);
						unsigned bsx = static_cast<unsigned>(diffMapBlockSizeW + 1);
						unsigned x = static_cast<unsigned>(bx * diffMapBlockSizeW);
						if (x + bsx - 1 >= w)
							bsx = w - x;
						for (unsigned j = 0; j < bsx; ++j)
						{
							unsigned x = static_cast<unsigned>(bx * diffMapBlockSizeW + j);
							scanline[x * 4 + 0] = Image::valueB(color);
							scanline[x * 4 + 1] = Image::valueG(color);
							scanline[x * 4 + 2] = Image::valueR(color);
							scanline[x * 4 + 3] = 0xff;
						}
					}
				}
			}
		}
		return &m_imgDiffMap;
	}

	Point<unsigned> GetImageOffset(int pane) const
	{
		if (pane < 0 || pane >= m_nImages)
			return Point<unsigned>();
		return m_offset[pane];
	}

	void AddImageOffset(int pane, int dx, int dy)
	{
		if (pane < 0 || pane >= m_nImages)
			return;
		int minx = INT_MAX, miny = INT_MAX;
		Point<int> offset[3];
		for (int i = 0; i < m_nImages; ++i)
		{
			offset[i].x = m_offset[i].x;
			offset[i].y = m_offset[i].y;
			if (i == pane)
			{
				offset[i].x += dx;
				offset[i].y += dy;
			}
			if (offset[i].x < minx)
				minx = offset[i].x;
			if (offset[i].y < miny)
				miny = offset[i].y;
		}
		for (int i = 0; i < m_nImages; ++i)
		{
			m_offset[i].x = offset[i].x - minx;
			m_offset[i].y = offset[i].y - miny;
		}
		CompareImages();
		RefreshImages();
	}

	bool ConvertToRealPos(int pane, int x, int y, int& rx, int& ry, bool clamp = true) const
	{
		x -= m_offset[pane].x;
		y -= m_offset[pane].y;

		bool inside = true;
		if (m_insertionDeletionDetectionMode == INSERTION_DELETION_DETECTION_NONE ||
			m_lineDiffInfos.size() == 0)
		{
			if (x < 0)
			{
				rx = clamp ? 0 : x;
				inside = false;
			}
			else if (x >= static_cast<int>(m_imgPreprocessed[pane].width()))
			{
				rx = clamp ? (m_imgPreprocessed[pane].width() - 1) : x;
				inside = false;
			}
			else
				rx = x;
			if (y < 0)
			{
				ry = clamp ? 0 : y;
				inside = false;
			}
			else if (y >= static_cast<int>(m_imgPreprocessed[pane].height()))
			{
				ry = clamp ? (m_imgPreprocessed[pane].height() - 1) : y;
				inside = false;
			}
			else
				ry = y;
			return inside;
		}

		if (m_insertionDeletionDetectionMode == INSERTION_DELETION_DETECTION_VERTICAL)
		{
			if (x < 0)
			{
				rx = clamp ? 0 : x;
				inside = false;
			}
			else if (x >= static_cast<int>(m_imgPreprocessed[pane].width()))
			{
				rx = clamp ? (m_imgPreprocessed[pane].width() - 1) : x;
				inside = false;
			}
			else
				rx = x;
			for (auto& lineDiff: m_lineDiffInfos)
			{
				if (y <= lineDiff.dend[pane])
				{
					if (y < 0)
					{
						ry = clamp ? 0 : y;
						inside = false;
					}
					else
						ry = y - lineDiff.dbegin + lineDiff.begin[pane];
					return inside;
				}
				else if (lineDiff.dend[pane] < y && y <= lineDiff.dendmax)
				{
					ry = lineDiff.end[pane];
					inside = false;
					return inside;
				}
			}
			ry = y - m_lineDiffInfos.back().dendmax + m_lineDiffInfos.back().end[pane];
			if (ry >= GetImageHeight(pane))
			{
				if (clamp)
					ry = GetImageHeight(pane) - 1;
				inside = false;
			}
			return inside;
		}
		else
		{
			if (y < 0)
			{
				ry = clamp ? 0 : y;
				inside = false;
			}
			else if (y >= static_cast<int>(m_imgPreprocessed[pane].height()))
			{
				ry = clamp ? (m_imgPreprocessed[pane].height() - 1) : y;
				inside = false;
			}
			else
				ry = y;
			for (auto& lineDiff: m_lineDiffInfos)
			{
				if (x <= lineDiff.dend[pane])
				{
					if (x < 0)
					{
						rx = clamp ? 0 : x;
						inside = false;
					}
					else
						rx = x - lineDiff.dbegin + lineDiff.begin[pane];
					return inside;
				}
				else if (lineDiff.dend[pane] < x && x <= lineDiff.dendmax)
				{
					rx = lineDiff.end[pane];
					inside = false;
					return inside;
				}
			}
			rx = x - m_lineDiffInfos.back().dendmax + m_lineDiffInfos.back().end[pane];
			if (rx >= GetImageWidth(pane))
			{
				if (clamp)
					rx = GetImageWidth(pane) - 1;
				inside = false;
			}
			return inside;
		}
	}

	void CopySubImage(int pane, int x, int y, int x2, int y2, Image& image)
	{
		TemporaryTransformation tmp(*this);
		m_imgOrig32[pane].copySubImage(image, x, y, x2, y2);
	}

protected:
	bool LoadImages()
	{
		bool bSucceeded = true;
		for (int i = 0; i < m_nImages; ++i)
		{
			m_imgConverter[i].close();
			m_currentPage[i] = 0;
			m_imgOrigMultiPage[i].load(m_filename[i]);
			if (m_imgOrigMultiPage[i].isValid() && m_imgOrigMultiPage[i].getPageCount() > 1)
			{
				m_imgOrig[i] = m_imgOrigMultiPage[i].getImage(0);
				m_imgOrig32[i] = m_imgOrig[i];
			}
			else
			{
				m_imgOrigMultiPage[i].close();
				if (ImgConverter::isSupportedImage(m_filename[i].c_str()))
				{
					if (m_imgConverter[i].load(m_filename[i].c_str()))
						m_imgConverter[i].render(m_imgOrig[i], 0, m_vectorImageZoomRatio);
				}
				if (!m_imgConverter[i].isValid())
				{
					if (!m_imgOrig[i].load(m_filename[i]))
						bSucceeded = false;
				}
				m_imgOrig32[i] = m_imgOrig[i];
				std::map<std::string, std::string> meta = m_imgOrig[i].getMetadata();
				m_horizontalFlip[i] = false;
				m_verticalFlip[i] = false;
				m_angle[i] = 0.f;
				std::string orientation = meta["EXIF_MAIN/Orientation"];
				if (orientation == "top, right side") // 2
					m_horizontalFlip[i] = true;
				else if (orientation == "bottom, right side") // 3
					m_angle[i] = 180.f;
				else if (orientation == "bottom, left side") // 4
					m_verticalFlip[i] = true;
				else if (orientation == "left side, top") // 5
				{
					m_angle[i] = 90.f;
					m_verticalFlip[i] = true;
				}
				else if (orientation == "right side, top") // 6
					m_angle[i] = 270.f;
				else if (orientation == "right side, bottom") // 7
				{
					m_angle[i] = 270.f;
					m_verticalFlip[i] = true;
				}
				else if (orientation == "left side, bottom") // 8
					m_angle[i] = 90.f;
			}
			m_imgOrig32[i].convertTo32Bits();
		}
		return bSucceeded;
	}

	void ChangePage(int pane, int page)
	{
		if (m_imgOrigMultiPage[pane].isValid() || m_imgConverter[pane].isValid())
		{
			if (m_imgOrigMultiPage[pane].isValid())
				m_imgOrig[pane] = m_imgOrigMultiPage[pane].getImage(page);
			else
				m_imgConverter[pane].render(m_imgOrig[pane], page, m_vectorImageZoomRatio);
			m_imgOrig32[pane] = m_imgOrig[pane];
			m_imgOrig32[pane].convertTo32Bits();
			if (m_currentDiffIndex >= 0)
				m_currentDiffIndex = 0;
		}
	}

	Size<unsigned> GetMaxWidthHeight()
	{
		unsigned wmax = 0;
		unsigned hmax = 0;
		for (int i = 0; i < m_nImages; ++i)
		{
			wmax = (std::max)(wmax, static_cast<unsigned>(m_imgPreprocessed[i].width())  + m_offset[i].x);
			hmax = (std::max)(hmax, static_cast<unsigned>(m_imgPreprocessed[i].height()) + m_offset[i].y);
		}
		return Size<unsigned>(wmax, hmax);
	}

	void InitializeDiff()
	{
		Size<unsigned> size = GetMaxWidthHeight();
		int nBlocksX = (size.cx + m_diffBlockSize - 1) / m_diffBlockSize;
		int nBlocksY = (size.cy + m_diffBlockSize - 1) / m_diffBlockSize;

		m_diff.clear();
		m_diff.resize(nBlocksX, nBlocksY);
		if (m_nImages == 3)
		{
			m_diff01.clear();
			m_diff01.resize(nBlocksX, nBlocksY);
			m_diff21.clear();
			m_diff21.resize(nBlocksX, nBlocksY);
			m_diff02.clear();
			m_diff02.resize(nBlocksX, nBlocksY);
		}
		m_diffInfos.clear();
	}

	void InitializeDiffImages()
	{
		Size<unsigned> size = GetMaxWidthHeight();
		for (int i = 0; i < m_nImages; ++i)
			m_imgDiff[i].setSize(size.cx, size.cy);
	}

	void CompareImages2(int pane1, int pane2, DiffBlocks& diff)
	{
		unsigned x1min = m_imgPreprocessed[pane1].width()  > 0 ? m_offset[pane1].x : -1;
		unsigned y1min = m_imgPreprocessed[pane1].height() > 0 ? m_offset[pane1].y : -1;
		unsigned x2min = m_imgPreprocessed[pane2].width()  > 0 ? m_offset[pane2].x : -1;
		unsigned y2min = m_imgPreprocessed[pane2].height() > 0 ? m_offset[pane2].y : -1;
		unsigned x1max = x1min + m_imgPreprocessed[pane1].width() - 1;
		unsigned y1max = y1min + m_imgPreprocessed[pane1].height() - 1;
		unsigned x2max = x2min + m_imgPreprocessed[pane2].width() - 1;
		unsigned y2max = y2min + m_imgPreprocessed[pane2].height() - 1;

		const unsigned wmax = (std::max)(x1max + 1, x2max + 1);
		const unsigned hmax = (std::max)(y1max + 1, y2max + 1);

		for (unsigned by = 0; by < diff.height(); ++by)
		{
			unsigned bsy = (hmax - by * m_diffBlockSize) >= m_diffBlockSize ? m_diffBlockSize : (hmax - by * m_diffBlockSize); 
			for (unsigned i = 0; i < bsy; ++i)
			{
				unsigned y = by * m_diffBlockSize + i;
				if (y < y1min || y > y1max || y < y2min || y > y2max)
				{
					for (unsigned bx = 0; bx < diff.width(); ++bx)
						diff(bx, by) = -1;
				}
				else
				{
					const unsigned char *scanline1 = m_imgPreprocessed[pane1].scanLine(y - y1min);
					const unsigned char *scanline2 = m_imgPreprocessed[pane2].scanLine(y - y2min);
					if (x1min == x2min && x1max == x2max && m_colorDistanceThreshold == 0.0)
					{
						if (memcmp(scanline1, scanline2, (x2max + 1 - x1min) * 4) == 0)
							continue;
					}
					for (unsigned x = 0; x < wmax; ++x)
					{
						if (x < x1min || x > x1max || x < x2min || x > x2max)
							diff(x / m_diffBlockSize, by) = -1;
						else
						{
							if (m_colorDistanceThreshold > 0.0)
							{
								int bdist = scanline1[(x - x1min) * 4 + 0] - scanline2[(x - x2min) * 4 + 0];
								int gdist = scanline1[(x - x1min) * 4 + 1] - scanline2[(x - x2min) * 4 + 1];
								int rdist = scanline1[(x - x1min) * 4 + 2] - scanline2[(x - x2min) * 4 + 2];
								int adist = scanline1[(x - x1min) * 4 + 3] - scanline2[(x - x2min) * 4 + 3];
								int colorDistance2 = rdist * rdist + gdist * gdist + bdist * bdist + adist * adist;
								if (colorDistance2 > m_colorDistanceThreshold * m_colorDistanceThreshold)
									diff(x / m_diffBlockSize, by) = -1;
							}
							else
							{
								if (scanline1[(x - x1min) * 4 + 0] != scanline2[(x - x2min) * 4 + 0] ||
									scanline1[(x - x1min) * 4 + 1] != scanline2[(x - x2min) * 4 + 1] ||
									scanline1[(x - x1min) * 4 + 2] != scanline2[(x - x2min) * 4 + 2] ||
									scanline1[(x - x1min) * 4 + 3] != scanline2[(x - x2min) * 4 + 3])
								{
									diff(x / m_diffBlockSize, by) = -1;
								}
							}
						}
					}
				}
			}
		}
	}
		
	void FloodFill8Directions(DiffBlocks& data, int x, int y, unsigned val)
	{
		std::vector<Point<int> > stack;
		stack.push_back(Point<int>(x, y));
		while (!stack.empty())
		{
			const Point<int>& pt = stack.back();
			const int x = pt.x;
			const int y = pt.y;
			stack.pop_back();
			if (data(x, y) != -1)
				continue;
			data(x, y) = val;
			if (x + 1 < static_cast<int>(data.width()))
			{
				stack.push_back(Point<int>(x + 1, y));
				if (y + 1 < static_cast<int>(data.height()))
					stack.push_back(Point<int>(x + 1, y + 1));
				if (y - 1 >= 0)
					stack.push_back(Point<int>(x + 1, y - 1));
			}
			if (x - 1 >= 0)
			{
				stack.push_back(Point<int>(x - 1, y));
				if (y + 1 < static_cast<int>(data.height()))
					stack.push_back(Point<int>(x - 1, y + 1));
				if (y - 1 >= 0)
					stack.push_back(Point<int>(x - 1, y - 1));
			}
			if (y + 1 < static_cast<int>(data.height()))
				stack.push_back(Point<int>(x, y + 1));
			if (y - 1 >= 0)
				stack.push_back(Point<int>(x, y - 1));
		}
	}

	int MarkDiffIndex(DiffBlocks& diff)
	{
		int diffCount = 0;
		for (unsigned by = 0; by < diff.height(); ++by)
		{
			for (unsigned bx = 0; bx < diff.width(); ++bx)
			{
				int idx = diff(bx, by);
				if (idx == -1)
				{
					m_diffInfos.push_back(DiffInfo(OP_DIFF, bx, by));
					++diffCount;
					FloodFill8Directions(diff, bx, by, diffCount);
				}
				else if (idx != 0)
				{
					Rect<int>& rc = m_diffInfos[idx - 1].rc;
					if (static_cast<int>(bx) < rc.left)
						rc.left = bx;
					else if (static_cast<int>(bx + 1) > rc.right)
						rc.right = bx + 1;
					if (static_cast<int>(by) < rc.top)
						rc.top = by;
					else if (static_cast<int>(by + 1) > rc.bottom)
						rc.bottom = by + 1;
				}
			}
		}
		return diffCount;
	}

	int MarkDiffIndex3way(const DiffBlocks& diff01, const DiffBlocks& diff21, const DiffBlocks& diff02, DiffBlocks& diff3)
	{
		int diffCount = MarkDiffIndex(diff3);
		std::vector<DiffStat> counter(m_diffInfos.size());
		for (unsigned by = 0; by < diff3.height(); ++by)
		{
			for (unsigned bx = 0; bx < diff3.width(); ++bx)
			{
				int diffIndex = diff3(bx, by);
				if (diffIndex == 0)
					continue;
				--diffIndex;
				if (diff21(bx, by) == 0)
					++counter[diffIndex].d1;
				else if (diff02(bx, by) == 0)
					++counter[diffIndex].d2;
				else if (diff01(bx, by) == 0)
					++counter[diffIndex].d3;
				else
					++counter[diffIndex].detc;
			}
		}
		
		for (size_t i = 0; i < m_diffInfos.size(); ++i)
		{
			int op;
			if (counter[i].d1 != 0 && counter[i].d2 == 0 && counter[i].d3 == 0 && counter[i].detc == 0)
				op = OP_1STONLY;
			else if (counter[i].d1 == 0 && counter[i].d2 != 0 && counter[i].d3 == 0 && counter[i].detc == 0)
				op = OP_2NDONLY;
			else if (counter[i].d1 == 0 && counter[i].d2 == 0 && counter[i].d3 != 0 && counter[i].detc == 0)
				op = OP_3RDONLY;
			else
				op = OP_DIFF;
			m_diffInfos[i].op = op;
		}
		return diffCount;
	}

	void Make3WayDiff(const DiffBlocks& diff01, const DiffBlocks& diff21, DiffBlocks& diff3)
	{
		diff3 = diff01;
		for (unsigned bx = 0; bx < diff3.width(); ++bx)
		{
			for (unsigned by = 0; by < diff3.height(); ++by)
			{
				if (diff21(bx, by) != 0)
					diff3(bx, by) = -1;
			}
		}
	}

	inline Image::Color GetDiffColorFromPosition(int pane, int x, int y, Image::Color diffColor, Image::Color diffDeletedColor)
	{
		x -= m_offset[pane].x;
		y -= m_offset[pane].y;

		if (m_insertionDeletionDetectionMode == INSERTION_DELETION_DETECTION_NONE || 
			m_lineDiffInfos.size() == 0 ||
			x < 0 || x >= static_cast<int>(m_imgPreprocessed[pane].width()) ||
			y < 0 || y >= static_cast<int>(m_imgPreprocessed[pane].height()))
			return diffColor;

		if (m_insertionDeletionDetectionMode == INSERTION_DELETION_DETECTION_VERTICAL)
		{
			for (auto& lineDiff: m_lineDiffInfos)
			{
				if (lineDiff.dbegin <= y && y <= lineDiff.dendmax)
					return diffDeletedColor;
			}
		}
		else
		{
			for (auto& lineDiff : m_lineDiffInfos)
			{
				if (lineDiff.dbegin <= x && x <= lineDiff.dendmax)
					return diffDeletedColor;
			}
		}
		return diffColor;
	}

	void MarkDiff(int pane, const DiffBlocks& diff)
	{
		const unsigned w = m_imgDiff[pane].width();
		const unsigned h = m_imgDiff[pane].height();

		for (unsigned by = 0; by < diff.height(); ++by)
		{
			for (unsigned bx = 0; bx < diff.width(); ++bx)
			{
				int diffIndex = diff(bx, by);
				if (diffIndex != 0 && (
					(pane == 0 && m_diffInfos[diffIndex - 1].op != OP_3RDONLY) ||
					(pane == 1) ||
					(pane == 2 && m_diffInfos[diffIndex - 1].op != OP_1STONLY)
					))
				{
					Image::Color color = (diffIndex - 1 == m_currentDiffIndex) ? m_selDiffColor : m_diffColor;
					Image::Color colorDeleted = (diffIndex - 1 == m_currentDiffIndex) ? m_selDiffDeletedColor : m_diffDeletedColor;
					unsigned bsy = (h - by * m_diffBlockSize < m_diffBlockSize) ? (h - by * m_diffBlockSize) : m_diffBlockSize;
					for (unsigned i = 0; i < bsy; ++i)
					{
						unsigned y = by * m_diffBlockSize + i;
						unsigned char *scanline = m_imgDiff[pane].scanLine(y);
						unsigned bsx = (w - bx * m_diffBlockSize < m_diffBlockSize) ? (w - bx * m_diffBlockSize) : m_diffBlockSize;
						for (unsigned j = 0; j < bsx; ++j)
						{
							unsigned x = bx * m_diffBlockSize + j;
							if (scanline[x * 4 + 3] != 0)
							{
								scanline[x * 4 + 0] = static_cast<unsigned char>(scanline[x * 4 + 0] * (1 - m_diffColorAlpha) + Image::valueB(color) * m_diffColorAlpha);
								scanline[x * 4 + 1] = static_cast<unsigned char>(scanline[x * 4 + 1] * (1 - m_diffColorAlpha) + Image::valueG(color) * m_diffColorAlpha);
								scanline[x * 4 + 2] = static_cast<unsigned char>(scanline[x * 4 + 2] * (1 - m_diffColorAlpha) + Image::valueR(color) * m_diffColorAlpha);
							}
							else
							{
								Image::Color dcolor = GetDiffColorFromPosition(pane, x, y, color, colorDeleted);
								scanline[x * 4 + 0] = Image::valueB(dcolor);
								scanline[x * 4 + 1] = Image::valueG(dcolor);
								scanline[x * 4 + 2] = Image::valueR(dcolor);
								scanline[x * 4 + 3] = static_cast<unsigned char>(0xff * m_diffColorAlpha);
							}
						}
					}
				}
			}
		}
	}

	void WipeEffect()
	{
		const unsigned w = m_imgDiff[0].width();
		const unsigned h = m_imgDiff[0].height();

		if (m_wipeMode == WIPE_VERTICAL)
		{
			auto tmp = new unsigned char[w * 4];
			for (unsigned y = m_wipePosition; y < h; ++y)
			{
				for (int pane = 0; pane < m_nImages - 1; ++pane)
				{
					unsigned char *scanline  = m_imgDiff[pane].scanLine(y);
					unsigned char *scanline2 = m_imgDiff[pane + 1].scanLine(y);
					memcpy(tmp, scanline, w * 4);
					memcpy(scanline, scanline2, w * 4);
					memcpy(scanline2, tmp, w * 4);
				}
			}
			delete[] tmp;
		}
		else if (m_wipeMode = WIPE_HORIZONTAL)
		{
			for (unsigned y = 0; y < h; ++y)
			{
				for (int pane = 0; pane < m_nImages - 1; ++pane)
				{
					unsigned char *scanline = m_imgDiff[pane].scanLine(y);
					unsigned char *scanline2 = m_imgDiff[pane + 1].scanLine(y);
					for (unsigned x = m_wipePosition; x < w; ++x)
					{
						unsigned char tmp[4];
						tmp[0] = scanline[x * 4 + 0];
						tmp[1] = scanline[x * 4 + 1];
						tmp[2] = scanline[x * 4 + 2];
						tmp[3] = scanline[x * 4 + 3];
						scanline[x * 4 + 0] = scanline2[x * 4 + 0];
						scanline[x * 4 + 1] = scanline2[x * 4 + 1];
						scanline[x * 4 + 2] = scanline2[x * 4 + 2];
						scanline[x * 4 + 3] = scanline2[x * 4 + 3];
						scanline2[x * 4 + 0] = tmp[0];
						scanline2[x * 4 + 1] = tmp[1];
						scanline2[x * 4 + 2] = tmp[2];
						scanline2[x * 4 + 3] = tmp[3];
					}
				}
			}
		}
	}

	void CopyPreprocessedImageToDiffImage(int dst)
	{
		unsigned w = m_imgPreprocessed[dst].width();
		unsigned h = m_imgPreprocessed[dst].height();
		unsigned offset_x = m_offset[dst].x;
		for (unsigned y = 0; y < h; ++y)
		{
			const unsigned char *scanline_src = m_imgPreprocessed[dst].scanLine(y);
			unsigned char *scanline_dst = m_imgDiff[dst].scanLine(y + m_offset[dst].y);
			for (unsigned x = 0; x < w; ++x)
			{
				scanline_dst[(x + offset_x) * 4 + 0] = scanline_src[x * 4 + 0];
				scanline_dst[(x + offset_x) * 4 + 1] = scanline_src[x * 4 + 1];
				scanline_dst[(x + offset_x) * 4 + 2] = scanline_src[x * 4 + 2];
				scanline_dst[(x + offset_x) * 4 + 3] = scanline_src[x * 4 + 3];
			}
		}	
	}

	void XorImages2(int src, int dst)
	{
		unsigned w = m_imgPreprocessed[src].width();
		unsigned h = m_imgPreprocessed[src].height();
		unsigned offset_x = m_offset[src].x;
		for (unsigned y = 0; y < h; ++y)
		{
			const unsigned char *scanline_src = m_imgPreprocessed[src].scanLine(y);
			unsigned char *scanline_dst = m_imgDiff[dst].scanLine(y + m_offset[src].y);
			for (unsigned x = 0; x < w; ++x)
			{
				scanline_dst[(x + offset_x) * 4 + 0] ^= scanline_src[x * 4 + 0];
				scanline_dst[(x + offset_x) * 4 + 1] ^= scanline_src[x * 4 + 1];
				scanline_dst[(x + offset_x) * 4 + 2] ^= scanline_src[x * 4 + 2];
			}
		}	
	}

	void AlphaBlendImages2(int src, int dst)
	{
		unsigned w = m_imgPreprocessed[src].width();
		unsigned h = m_imgPreprocessed[src].height();
		unsigned offset_x = m_offset[src].x;
		double overlayAlpha = m_overlayAlpha;
		if (m_overlayMode == OVERLAY_ALPHABLEND_ANIM)
		{
			auto now = std::chrono::system_clock::now();
			auto tse = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
			double t = static_cast<double>(tse.count() % OVERLAY_ALPHABLEND_ANIM_TIME);
			if (t < OVERLAY_ALPHABLEND_ANIM_TIME * 2 / 10)
				overlayAlpha = t / (OVERLAY_ALPHABLEND_ANIM_TIME * 2 / 10);
			else if (t < OVERLAY_ALPHABLEND_ANIM_TIME * 5 / 10)
				overlayAlpha = 1.0;
			else if (t < OVERLAY_ALPHABLEND_ANIM_TIME * 7 / 10)
				overlayAlpha = ((OVERLAY_ALPHABLEND_ANIM_TIME * 2 / 10) - (t - (OVERLAY_ALPHABLEND_ANIM_TIME * 5 / 10)))
				              / (OVERLAY_ALPHABLEND_ANIM_TIME * 2 / 10);
			else
				overlayAlpha = 0.0;
		}
		for (unsigned y = 0; y < h; ++y)
		{
			const unsigned char *scanline_src = m_imgPreprocessed[src].scanLine(y);
			unsigned char *scanline_dst = m_imgDiff[dst].scanLine(y + m_offset[src].y);
			for (unsigned x = 0; x < w; ++x)
			{
				scanline_dst[(x + offset_x) * 4 + 0] = static_cast<unsigned char>(scanline_dst[(x + offset_x) * 4 + 0] * (1 - overlayAlpha) + scanline_src[x * 4 + 0] * overlayAlpha);
				scanline_dst[(x + offset_x) * 4 + 1] = static_cast<unsigned char>(scanline_dst[(x + offset_x) * 4 + 1] * (1 - overlayAlpha) + scanline_src[x * 4 + 1] * overlayAlpha);
				scanline_dst[(x + offset_x) * 4 + 2] = static_cast<unsigned char>(scanline_dst[(x + offset_x) * 4 + 2] * (1 - overlayAlpha) + scanline_src[x * 4 + 2] * overlayAlpha);
				scanline_dst[(x + offset_x) * 4 + 3] = static_cast<unsigned char>(scanline_dst[(x + offset_x) * 4 + 3] * (1 - overlayAlpha) + scanline_src[x * 4 + 3] * overlayAlpha);
			}
		}	
	}

	void CopyImageWithGhostLine(const std::vector<LineDiffInfo>& lineDiffInfos, int npanes, Image src[], Image dst[])
	{
		unsigned nlines;
		if (lineDiffInfos.size() == 0)
		{
			nlines = src[0].height();
		}
		else
		{
			const LineDiffInfo& lastLineDiff = lineDiffInfos.back();
			nlines = (lastLineDiff.dendmax + 1) + src[0].height() - (lastLineDiff.end[0] + 1);
		}

		int ydst = 0;
		for (int pane = 0; pane < npanes; ++pane)
			dst[pane].setSize(src[pane].width(), nlines);
		for (size_t i = 0; i < lineDiffInfos.size(); ++i)
		{
			const LineDiffInfo& lineDiffInfo = lineDiffInfos[i];

			for (int pane = 0; pane < npanes; ++pane)
			{
				int orgydst = ydst;
				for (int ysrc = (i > 0) ? (lineDiffInfos[i - 1].end[pane] + 1) : 0; ysrc < lineDiffInfo.begin[pane]; ++ysrc)
					memcpy(dst[pane].scanLine(ydst++), src[pane].scanLine(ysrc), src[pane].width() * 4);
				ydst = orgydst;
			}

			ydst = lineDiffInfo.dbegin;
			for (int pane = 0; pane < npanes; ++pane)
			{
				int orgydst = ydst;
				for (int ysrc = lineDiffInfo.begin[pane]; ysrc <= lineDiffInfo.end[pane]; ++ysrc)
					memcpy(dst[pane].scanLine(ydst++), src[pane].scanLine(ysrc), src[pane].width() * 4);
				ydst = orgydst;
			}
			ydst = lineDiffInfo.dendmax + 1;
		}

		for (int pane = 0; pane < npanes; ++pane)
		{
			int orgydst = ydst;
			for (int ysrc = (lineDiffInfos.size() > 0) ? (lineDiffInfos[lineDiffInfos.size() - 1].end[pane] + 1) : 0;
				ysrc < static_cast<int>(src[pane].height()); ++ysrc)
				memcpy(dst[pane].scanLine(ydst++), src[pane].scanLine(ysrc), src[pane].width() * 4);
			ydst = orgydst;
		}
	}

	std::vector<LineDiffInfo> MakeLineDiff(const Image& img1, const Image& img2)
	{
		DataForDiff data1(img1, m_colorDistanceThreshold);
		DataForDiff data2(img2, m_colorDistanceThreshold);
		Diff<DataForDiff> diff(data1, data2);
		std::vector<char> edscript;
		std::vector<LineDiffInfo> lineDiffInfosTmp;
		std::vector<LineDiffInfo> lineDiffInfos;

		diff.diff(static_cast<Diff<DataForDiff>::Algorithm>(m_diffAlgorithm), edscript);

		int i = 0, j = 0;
		for (auto ed : edscript)
		{
			switch (ed)
			{
			case '-':
				lineDiffInfosTmp.emplace_back(i, i, j, j - 1);
				++i;
				break;
			case '+':
				lineDiffInfosTmp.emplace_back(i, i - 1, j, j);
				++j;
				break;
			case '!':
				lineDiffInfosTmp.emplace_back(i, i, j, j);
				++i;
				++j;
				break;
			default:
				++i;
				++j;
				break;
			}
		}

		lineDiffInfos.clear();
		for (size_t i = 0; i < lineDiffInfosTmp.size(); ++i)
		{
			bool skipIt = false;
			// combine it with next ?
			if (i + 1 < (int)lineDiffInfosTmp.size())
			{
				if (lineDiffInfosTmp[i].end[0] + 1 == lineDiffInfosTmp[i + 1].begin[0]
					&& lineDiffInfosTmp[i].end[1] + 1 == lineDiffInfosTmp[i + 1].begin[1])
				{
					// diff[i] and diff[i+1] are contiguous
					// so combine them into diff[i+1] and ignore diff[i]
					lineDiffInfosTmp[i + 1].begin[0] = lineDiffInfosTmp[i].begin[0];
					lineDiffInfosTmp[i + 1].begin[1] = lineDiffInfosTmp[i].begin[1];
					skipIt = true;
				}
			}
			if (!skipIt)
			{
				// Should never have a pair where both are missing
				assert(lineDiffInfosTmp[i].begin[0] >= 0 || lineDiffInfosTmp[i].begin[1] >= 0);

				// Store the diff[i] in the caller list (m_pDiffs)
				lineDiffInfos.push_back(lineDiffInfosTmp[i]);
			}
		}

		return lineDiffInfos;
	}

	unsigned PrimeLineDiffInfos(std::vector<LineDiffInfo>& lineDiffInfos, int npanes, unsigned height0)
	{
		unsigned dlines = 0;
		for (size_t i = 0; i < lineDiffInfos.size(); ++i)
		{
			dlines += lineDiffInfos[i].begin[0] - ((i > 0) ? (lineDiffInfos[i - 1].end[0] + 1) : 0);

			LineDiffInfo& lineDiffInfo = lineDiffInfos[i];
			lineDiffInfo.dbegin = dlines;
			lineDiffInfo.dendmax = 0;
			for (int pane = 0; pane < npanes; ++pane)
			{
				lineDiffInfo.dend[pane] = lineDiffInfo.dbegin + lineDiffInfo.end[pane] - lineDiffInfo.begin[pane];
				if (lineDiffInfo.dendmax < lineDiffInfo.dbegin + lineDiffInfo.end[pane] - lineDiffInfo.begin[pane])
					lineDiffInfo.dendmax = lineDiffInfo.dbegin + lineDiffInfo.end[pane] - lineDiffInfo.begin[pane];
			}
			dlines = lineDiffInfo.dendmax + 1;
		}
		dlines += height0 - (lineDiffInfos.size() > 0 ? lineDiffInfos[lineDiffInfos.size() - 1].end[0] + 1 : 0);
		return dlines;
	}

	class TemporaryTransformation
	{
	public:
		TemporaryTransformation(CImgDiffBuffer& buffer)
			: m_buffer(buffer)
		{
			m_buffer.TransformImages(false);
		}

		~TemporaryTransformation()
		{
			m_buffer.TransformImages(true);
		}
	private:
		CImgDiffBuffer& m_buffer;
	};

	void TransformImages(bool reverse)
	{
		m_temporarilyTransformed = !reverse;
		for (int pane = 0; pane < m_nImages; ++pane)
		{
			if (!reverse)
			{
				if (m_horizontalFlip[pane])
					m_imgOrig32[pane].flipHorizontal();
				if (m_verticalFlip[pane])
					m_imgOrig32[pane].flipVertical();
				if (m_angle[pane])
					m_imgOrig32[pane].rotate(m_angle[pane]);
			}
			else
			{
				if (m_angle[pane])
					m_imgOrig32[pane].rotate(-m_angle[pane]);
				if (m_horizontalFlip[pane])
					m_imgOrig32[pane].flipHorizontal();
				if (m_verticalFlip[pane])
					m_imgOrig32[pane].flipVertical();
			}
		}
	}

	void PreprocessImages()
	{
		auto compfunc02 = [&](const LineDiffInfo & wd3) {
			unsigned wlen0 = wd3.end[0] + 1 - wd3.begin[0];
			unsigned wlen2 = wd3.end[2] + 1 - wd3.begin[2];
			if (wlen0 != wlen2)
				return false;
			for (unsigned i = 0; i < wlen0; ++i)
			{
				if (!alineEquals(
					m_imgOrig32[0], m_imgOrig32[2],
					wd3.begin[0] + i, wd3.begin[2] + i,
					m_colorDistanceThreshold))
					return false;
			}
			return true;
		};
		
		TemporaryTransformation tmp(*this);

		std::vector<LineDiffInfo> lineDiffInfos10, lineDiffInfos12;
		switch (m_insertionDeletionDetectionMode)
		{
		case INSERTION_DELETION_DETECTION_VERTICAL:
		{
			if (m_nImages == 2)
				 m_lineDiffInfos = MakeLineDiff(m_imgOrig32[0], m_imgOrig32[1]);
			else
			{
				lineDiffInfos10 = MakeLineDiff(m_imgOrig32[1], m_imgOrig32[0]);
				lineDiffInfos12 = MakeLineDiff(m_imgOrig32[1], m_imgOrig32[2]);
				m_lineDiffInfos = ::Make3WayLineDiff(lineDiffInfos10, lineDiffInfos12, compfunc02);
			}
			PrimeLineDiffInfos(m_lineDiffInfos, m_nImages, m_imgOrig32[0].height());
			CopyImageWithGhostLine(m_lineDiffInfos, m_nImages, m_imgOrig32, m_imgPreprocessed);
			break;
		}
		case INSERTION_DELETION_DETECTION_HORIZONTAL:
		{
			Image imgTransposed[3] = { m_imgOrig32[0], m_imgOrig32[1], m_imgOrig32[2] };
			for (int pane = 0; pane < m_nImages; ++pane)
				imgTransposed[pane].rotate(-90);
			if (m_nImages == 2)
				m_lineDiffInfos = MakeLineDiff(imgTransposed[0], imgTransposed[1]);
			else
			{
				lineDiffInfos10 = MakeLineDiff(imgTransposed[1], imgTransposed[0]);
				lineDiffInfos12 = MakeLineDiff(imgTransposed[1], imgTransposed[2]);
				m_lineDiffInfos = ::Make3WayLineDiff(lineDiffInfos10, lineDiffInfos12, compfunc02);
			}
			PrimeLineDiffInfos(m_lineDiffInfos, m_nImages, m_imgOrig32[0].height());
			CopyImageWithGhostLine(m_lineDiffInfos, m_nImages, imgTransposed, m_imgPreprocessed);
			for (int pane = 0; pane < m_nImages; ++pane)
				m_imgPreprocessed[pane].rotate(90);
			break;
		}
		default:
			m_lineDiffInfos.clear();
			for (int i = 0; i < m_nImages; ++i)
				m_imgPreprocessed[i] = m_imgOrig32[i];
			break;
		}
	}

	int m_nImages;
	MultiPageImages m_imgOrigMultiPage[3];
	Point<unsigned> m_offset[3];
	Image m_imgOrig[3];
	Image m_imgOrig32[3];
	Image m_imgPreprocessed[3];
	Image m_imgDiff[3];
	Image m_imgDiffMap;
	ImgConverter m_imgConverter[3];
	std::wstring m_filename[3];
	bool m_showDifferences;
	bool m_blinkDifferences;
	float m_vectorImageZoomRatio;
	INSERTION_DELETION_DETECTION_MODE m_insertionDeletionDetectionMode;
	OVERLAY_MODE m_overlayMode;
	double m_overlayAlpha;
	WIPE_MODE m_wipeMode;
	int m_wipePosition;
	unsigned m_diffBlockSize;
	Image::Color m_selDiffColor;
	Image::Color m_selDiffDeletedColor;
	Image::Color m_diffColor;
	Image::Color m_diffDeletedColor;
	double m_diffColorAlpha;
	double m_colorDistanceThreshold;
	float m_angle[3];
	bool m_horizontalFlip[3];
	bool m_verticalFlip[3];
	int m_currentPage[3];
	int m_currentDiffIndex;
	int m_diffCount;
	DiffBlocks m_diff, m_diff01, m_diff21, m_diff02;
	std::vector<DiffInfo> m_diffInfos;
	std::vector<LineDiffInfo> m_lineDiffInfos;
	bool m_temporarilyTransformed;
	DIFF_ALGORITHM m_diffAlgorithm;
};
