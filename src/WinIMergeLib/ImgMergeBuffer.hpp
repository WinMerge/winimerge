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

#include "ImgDiffBuffer.hpp"

struct UndoRecord
{
	UndoRecord(int pane, Image *oldbitmap, Image *newbitmap, const int modcountnew[3]) : 
		pane(pane), oldbitmap(oldbitmap), newbitmap(newbitmap)
	{
		for (int i = 0; i < 3; ++i)
			modcount[i] = modcountnew[i];
	}
	int pane;
	int modcount[3];
	Image *oldbitmap, *newbitmap;
};

struct UndoRecords
{
	UndoRecords() : m_currentUndoBufIndex(-1)
	{
		clear();
	}

	~UndoRecords()
	{
		clear();
	}

	void push_back(int pane, Image *oldbitmap, Image *newbitmap)
	{
		++m_currentUndoBufIndex;
		while (m_currentUndoBufIndex < static_cast<int>(m_undoBuf.size()))
		{
			--m_modcount[m_undoBuf.back().pane];
			delete m_undoBuf.back().newbitmap;
			delete m_undoBuf.back().oldbitmap;
			m_undoBuf.pop_back();
		}
		++m_modcount[pane];
		m_undoBuf.push_back(UndoRecord(pane, oldbitmap, newbitmap, m_modcount));
	}

	const UndoRecord& undo()
	{
		if (m_currentUndoBufIndex < 0)
			throw "no undoable";
		const UndoRecord& rec = m_undoBuf[m_currentUndoBufIndex];
		--m_currentUndoBufIndex;
		return rec;
	}

	const UndoRecord& redo()
	{
		if (m_currentUndoBufIndex >= static_cast<int>(m_undoBuf.size()) - 1)
			throw "no redoable";
		++m_currentUndoBufIndex;
		const UndoRecord& rec = m_undoBuf[m_currentUndoBufIndex];
		return rec;
	}

	bool is_modified(int pane) const
	{
		if (m_currentUndoBufIndex < 0)
			return (m_modcountonsave[pane] != 0);
		else
			return (m_modcountonsave[pane] != m_undoBuf[m_currentUndoBufIndex].modcount[pane]);
	}

	void save(int pane)
	{
		if (m_currentUndoBufIndex < 0)
			m_modcountonsave[pane] = 0;
		else
			m_modcountonsave[pane] = m_undoBuf[m_currentUndoBufIndex].modcount[pane];
	}

	bool undoable() const
	{
		return (m_currentUndoBufIndex >= 0);
	}

	bool redoable() const
	{
		return (m_currentUndoBufIndex < static_cast<int>(m_undoBuf.size()) - 1);
	}

	void clear()
	{
		m_currentUndoBufIndex = -1;
		for (int i = 0; i < 3; ++i)
		{
			m_modcount[i] = 0;
			m_modcountonsave[i] = 0;
		}
		while (!m_undoBuf.empty())
		{
			delete m_undoBuf.back().newbitmap;
			delete m_undoBuf.back().oldbitmap;
			m_undoBuf.pop_back();
		}
	}

	int get_savepoint(int pane) const
	{
		return m_modcountonsave[pane];
	}

	void set_savepoint(int pane, int pos)
	{
		m_modcountonsave[pane] = pos;
	}

	std::vector<UndoRecord> m_undoBuf;
	int m_currentUndoBufIndex;
	int m_modcount[3];
	int m_modcountonsave[3];
};

class CImgMergeBuffer : public CImgDiffBuffer
{
public:
	CImgMergeBuffer()
	{
		for (int i = 0; i < 3; ++i)
			m_bRO[i] = false;
	}

	virtual ~CImgMergeBuffer()
	{
	}

	bool NewImages(int nImages, int nPages, int width, int height)
	{
		CloseImages();
		m_nImages = nImages;
		for (int i = 0; i < nImages; ++i)
			m_filename[i] = L"";
		for (int i = 0; i < m_nImages; ++i)
		{
			m_imgConverter[i].close();
			m_currentPage[i] = 0;
			if (nPages > 1)
			{
				for (int page = 0; page < nPages; ++page)
				{
					Image img{ width, height };
					m_imgOrigMultiPage[i].insertPage(page, img);
				}
				m_imgOrig[i] = m_imgOrigMultiPage[i].getImage(0);
				m_imgOrig32[i] = m_imgOrig[i];
			}
			else
			{
				m_imgOrigMultiPage[i].close();
				m_imgOrig[i] = Image{ width, height };
				m_imgOrig32[i] = m_imgOrig[i];
			}
		}
		return true;
	}

	bool Resize(int pane, int width, int height)
	{
		if (width == m_imgOrig32[pane].width() && height == m_imgOrig32[pane].height())
			return false;

		Image *oldbitmap = new Image(m_imgOrig32[pane]);

		{
			TemporaryTransformation tmp(*this);
			m_imgOrig32[pane].setSize(width, height);
			PasteImageInternal(pane, 0, 0, *oldbitmap);
		}

		Image *newbitmap = new Image(m_imgOrig32[pane]);

		m_undoRecords.push_back(pane, oldbitmap, newbitmap);

		CompareImages();

		return true;
	}

	bool GetReadOnly(int pane) const
	{
		if (pane < 0 || pane >= m_nImages)
			return true;
		return m_bRO[pane];
	}

	void SetReadOnly(int pane, bool readOnly)
	{
		if (pane < 0 || pane >= m_nImages)
			return;
		m_bRO[pane] = readOnly;
	}

	void CopyDiff(int diffIndex, int srcPane, int dstPane)
	{
		if (srcPane < 0 || srcPane >= m_nImages)
			return;
		if (dstPane < 0 || dstPane >= m_nImages)
			return;
		if (diffIndex < 0 || diffIndex >= m_diffCount)
			return;
		if (m_bRO[dstPane])
			return;
		if (srcPane == dstPane)
			return;

		Image *oldbitmap = new Image(m_imgOrig32[dstPane]);

		{
			TemporaryTransformation tmp(*this);
			CopyDiffInternal(diffIndex, srcPane, dstPane);
		}

		Image *newbitmap = new Image(m_imgOrig32[dstPane]);
		m_undoRecords.push_back(dstPane, oldbitmap, newbitmap);
		CompareImages();
	}

	void CopyDiffAll(int srcPane, int dstPane)
	{
		if (srcPane < 0 || srcPane >= m_nImages)
			return;
		if (dstPane < 0 || dstPane >= m_nImages)
			return;
		if (m_bRO[dstPane])
			return;
		if (srcPane == dstPane)
			return;

		Image *oldbitmap = new Image(m_imgOrig32[dstPane]);

		{
			TemporaryTransformation tmp(*this);
			for (int diffIndex = 0; diffIndex < m_diffCount; ++diffIndex)
				CopyDiffInternal(diffIndex, srcPane, dstPane);
		}

		Image *newbitmap = new Image(m_imgOrig32[dstPane]);
		m_undoRecords.push_back(dstPane, oldbitmap, newbitmap);
		CompareImages();
	}

	int CopyDiff3Way(int dstPane)
	{
		if (dstPane < 0 || dstPane >= m_nImages)
			return 0;
		if (m_bRO[dstPane])
			return 0;

		Image *oldbitmap = new Image(m_imgOrig32[dstPane]);
		int nMerged = 0;
		{
			TemporaryTransformation tmp(*this);
			for (int diffIndex = 0; diffIndex < m_diffCount; ++diffIndex)
			{
				int srcPane;
				switch (m_diffInfos[diffIndex].op)
				{
				case OP_1STONLY:
					if (dstPane == 1)
						srcPane = 0;
					else
						srcPane = -1;
					break;
				case OP_2NDONLY:
					if (dstPane != 1)
						srcPane = 1;
					else
						srcPane = -1;
					break;
				case OP_3RDONLY:
					if (dstPane == 1)
						srcPane = 2;
					else
						srcPane = -1;
					break;
				case OP_DIFF:
					srcPane = -1;
					break;
				}

				if (srcPane >= 0)
				{
					CopyDiffInternal(diffIndex, srcPane, dstPane);
					++nMerged;
				}
			}
		}

		Image *newbitmap = new Image(m_imgOrig32[dstPane]);
		m_undoRecords.push_back(dstPane, oldbitmap, newbitmap);
		CompareImages();

		return nMerged;
	}

	bool DeleteRectangle(int pane, int left, int top, int right, int bottom)
	{
		if (pane < 0 || pane >= m_nImages || m_bRO[pane])
			return false;

		Image *oldbitmap = new Image(m_imgOrig32[pane]);

		{
			TemporaryTransformation tmp(*this);
			for (unsigned i = top; i < static_cast<unsigned>(bottom); ++i)
			{
				unsigned char* scanline = m_imgOrig32[pane].scanLine(i);
				memset(scanline + left * 4, 0, (right - left) * 4);
			}
		}

		Image *newbitmap = new Image(m_imgOrig32[pane]);
		m_undoRecords.push_back(pane, oldbitmap, newbitmap);

		CompareImages();
		return true;
	}

	bool IsModified(int pane) const
	{
		return m_undoRecords.is_modified(pane);
	}

	bool IsUndoable() const
	{
		return m_undoRecords.undoable();
	}

	bool IsRedoable() const
	{
		return m_undoRecords.redoable();
	}

	bool Undo()
	{
		if (!m_undoRecords.undoable())
			return false;
		const UndoRecord& rec = m_undoRecords.undo();
		m_imgOrig32[rec.pane] = *rec.oldbitmap;
		CompareImages();
		return true;
	}

	bool Redo()
	{
		if (!m_undoRecords.redoable())
			return false;
		const UndoRecord& rec = m_undoRecords.redo();
		m_imgOrig32[rec.pane] = *rec.newbitmap;
		CompareImages();
		return true;
	}

	int GetSavePoint(int pane) const
	{
		if (pane < 0 || pane >= m_nImages)
			return 0;
		return m_undoRecords.get_savepoint(pane);
	}

	void SetSavePoint(int pane, int pos)
	{
		if (pane < 0 || pane >= m_nImages)
			return;
		m_undoRecords.set_savepoint(pane, pos);
	}

	bool IsSaveSupported(int pane) const
	{
		return !m_imgConverter[pane].isValid() && m_imgOrig[pane].isSaveSupported();
	}

	int GetBlinkInterval() const
	{
		return m_blinkInterval;
	}

	void SetBlinkInterval(int interval)
	{
		m_blinkInterval = interval;
	}

	int GetOverlayAnimationInterval() const
	{
		return m_overlayAnimationInterval;
	}

	void SetOverlayAnimationInterval(int interval)
	{
		m_overlayAnimationInterval = interval;
	}


	bool SaveImage(int pane)
	{
		if (pane < 0 || pane >= m_nImages)
			return false;
		if (m_bRO[pane])
			return false;
		if (!m_undoRecords.is_modified(pane))
			return true;
		bool result = SaveImageAs(pane, (m_filename[pane] + (m_imgConverter[pane].isValid() ? L".png" : L"")).c_str());
		if (result)
			m_undoRecords.save(pane);
		return result;
	}

	bool SaveImages()
	{
		for (int i = 0; i < m_nImages; ++i)
			if (!SaveImage(i))
				return false;
		return true;
	}

	bool SaveImageAs(int pane, const wchar_t *filename)
	{
		if (pane < 0 || pane >= m_nImages)
			return false;
		m_imgOrig[pane].pullImageKeepingBPP(m_imgOrig32[pane]);
		int savedErrno = errno;
		errno = 0;
		if (m_imgOrigMultiPage[pane].isValid())
		{
			m_imgOrigMultiPage[pane].replacePage(m_currentPage[pane], m_imgOrig[pane]);
			if (!m_imgOrigMultiPage[pane].save(filename))
			{
				m_lastErrorCode = errno != 0 ? errno : ENOTSUP;
				if (errno == 0)
					errno = savedErrno;
				return false;
			}
		}
		else
		{
			if (!m_imgOrig[pane].save(filename))
			{
				m_lastErrorCode = errno != 0 ? errno : ENOTSUP;
				if (errno == 0)
					errno = savedErrno;
				return false;
			}
		}
		m_undoRecords.save(pane);
		m_filename[pane] = filename;
		return true;
	}

	virtual bool CloseImages() override
	{
		for (int i = 0; i < m_nImages; ++i)
			m_undoRecords.clear();
		return CImgDiffBuffer::CloseImages();
	}

	void PasteImage(int pane, int x, int y, const Image& image)
	{
		if (pane < 0 || pane >= m_nImages)
			return;

		Image *oldbitmap = new Image(m_imgOrig32[pane]);
		{
			TemporaryTransformation tmp(*this);
			PasteImageInternal(pane, x, y, image);
		}
		Image *newbitmap = new Image(m_imgOrig32[pane]);
		m_undoRecords.push_back(pane, oldbitmap, newbitmap);
		CompareImages();
	}

protected:
	void InsertRows(int pane, int y, int rows)
	{
		assert(m_temporarilyTransformed);
		Image tmpImage = m_imgOrig32[pane];
		m_imgOrig32[pane].setSize(tmpImage.width(), tmpImage.height() + rows);
		for (int i = 0; i < y; ++i)
			memcpy(m_imgOrig32[pane].scanLine(i), tmpImage.scanLine(i), tmpImage.width() * 4);
		for (unsigned i = y; i < tmpImage.height(); ++i)
			memcpy(m_imgOrig32[pane].scanLine(i + rows), tmpImage.scanLine(i), tmpImage.width() * 4);
	}

	void DeleteRows(int pane, int y, int rows)
	{
		assert(m_temporarilyTransformed);
		Image tmpImage = m_imgOrig32[pane];
		m_imgOrig32[pane].setSize(tmpImage.width(), tmpImage.height() - rows);
		for (int i = 0; i < y; ++i)
			memcpy(m_imgOrig32[pane].scanLine(i), tmpImage.scanLine(i), tmpImage.width() * 4);
		for (unsigned i = y + rows; i < tmpImage.height(); ++i)
			memcpy(m_imgOrig32[pane].scanLine(i - rows), tmpImage.scanLine(i), tmpImage.width() * 4);
	}

	void InsertColumns(int pane, int x, int columns)
	{
		assert(m_temporarilyTransformed);
		Image tmpImage = m_imgOrig32[pane];
		m_imgOrig32[pane].setSize(tmpImage.width() + columns, tmpImage.height());
		for (unsigned i = 0; i < tmpImage.height(); ++i)
		{
			memcpy(m_imgOrig32[pane].scanLine(i), tmpImage.scanLine(i), x * 4);
			memcpy(m_imgOrig32[pane].scanLine(i) + (x + columns) * 4, tmpImage.scanLine(i) + x * 4, (tmpImage.width() - x) * 4);
		}
	}

	void DeleteColumns(int pane, int x, int columns)
	{
		assert(m_temporarilyTransformed);
		Image tmpImage = m_imgOrig32[pane];
		m_imgOrig32[pane].setSize(tmpImage.width() - columns, tmpImage.height());
		for (unsigned i = 0; i < tmpImage.height(); ++i)
		{
			memcpy(m_imgOrig32[pane].scanLine(i), tmpImage.scanLine(i), x * 4);
			memcpy(m_imgOrig32[pane].scanLine(i) + x * 4, tmpImage.scanLine(i) + (x + columns) * 4, (tmpImage.width() - x - columns) * 4);
		}
	}

	void PasteImageInternal(int pane, int x, int y, const Image& image)
	{
		assert(m_temporarilyTransformed);
		if (pane < 0 || pane >= m_nImages)
			return;

		int width = m_imgOrig32[pane].width();
		int height = m_imgOrig32[pane].height();
		if (width == 0 || height == 0)
			return;

		int left = std::clamp(x, 0, width - 1);
		int top = std::clamp(y, 0, height - 1);
		int right = std::clamp(static_cast<int>(x + image.width()), 0, width);
		int bottom = std::clamp(static_cast<int>(y + image.height()), 0, height);
		if (right - left <= 0)
			return;
		if (bottom - top <= 0)
			return;

		for (int i = top; i < bottom; ++i)
			memcpy(m_imgOrig32[pane].scanLine(i) + left * 4, 
				image.scanLine(i - y) + (left - x) * 4, (right - left) * 4);
	}

	void CopyDiffInternal(int diffIndex, int srcPane, int dstPane)
	{
		assert(m_temporarilyTransformed);
		if (srcPane < 0 || srcPane >= m_nImages)
			return;
		if (dstPane < 0 || dstPane >= m_nImages)
			return;
		if (diffIndex < 0 || diffIndex >= m_diffCount)
			return;
		if (m_bRO[dstPane])
			return;

		Image imgTemp;
		const Rect<int>& rc = m_diffInfos[diffIndex].rc;
		unsigned xmin = rc.left   * m_diffBlockSize;
		if (xmin < m_offset[srcPane].x)
			xmin = m_offset[srcPane].x;
		unsigned ymin = rc.top    * m_diffBlockSize;
		if (ymin < m_offset[srcPane].y)
			ymin = m_offset[srcPane].y;
		unsigned xmax = rc.right  * m_diffBlockSize - 1;
		if (xmax >= m_imgPreprocessed[srcPane].width()  + m_offset[srcPane].x)
			xmax = m_imgPreprocessed[srcPane].width()  + m_offset[srcPane].x - 1;
		unsigned ymax = rc.bottom * m_diffBlockSize - 1;
		if (ymax >= m_imgPreprocessed[srcPane].height() + m_offset[srcPane].y)
			ymax = m_imgPreprocessed[srcPane].height() + m_offset[srcPane].y - 1;
		unsigned dsx = 0, dsy = 0, ox = 0, oy = 0;
		if (xmin < m_offset[dstPane].x)
			ox = m_offset[dstPane].x - xmin, dsx += ox;
		if (ymin < m_offset[dstPane].y)
			oy = m_offset[dstPane].y - ymin, dsy += oy;
		if (xmax >= m_imgPreprocessed[dstPane].width() + m_offset[dstPane].x)
			dsx += xmax - (m_imgPreprocessed[dstPane].width()  + m_offset[dstPane].x - 1);
		if (ymax >= m_imgPreprocessed[dstPane].height() + m_offset[dstPane].y)
			dsy += ymax - (m_imgPreprocessed[dstPane].height() + m_offset[dstPane].y - 1);
		if (dsx > 0 || dsy > 0)
		{
			imgTemp = m_imgOrig32[dstPane];
			m_imgOrig32[dstPane].setSize(m_imgOrig32[dstPane].width() + dsx, m_imgOrig32[dstPane].height() + dsy);
			m_imgOrig32[dstPane].pasteSubImage(imgTemp, ox, oy);
			m_offset[dstPane].x -= ox;
			m_offset[dstPane].y -= oy;
		}

		for (unsigned y = rc.top * m_diffBlockSize; y < rc.bottom * m_diffBlockSize; y += m_diffBlockSize)
		{
			for (unsigned x = rc.left * m_diffBlockSize; x < rc.right * m_diffBlockSize; x += m_diffBlockSize)
			{
				if (m_diff(x / m_diffBlockSize, y / m_diffBlockSize) == diffIndex + 1)
				{
					for (unsigned i = 0; i < m_diffBlockSize; ++i)
					{
						for (unsigned j = 0; j < m_diffBlockSize; ++j)
						{
							int rsx, rsy;
							if (ConvertToRealPos(srcPane, x + j, y + i, rsx, rsy))
							{
								int rdx, rdy;
								if (ConvertToRealPos(dstPane, x + j, y + i, rdx, rdy))
								{
									const unsigned char* scanline_src = m_imgOrig32[srcPane].scanLine(rsy);
									unsigned char* scanline_dst = m_imgOrig32[dstPane].scanLine(rdy);
									memcpy(&scanline_dst[rdx * 4], &scanline_src[rsx * 4], 4);
								}
							}
						}
					}
				}
			}
		}

		if (!std::all_of(std::begin(m_offset), std::end(m_offset), [](auto& pt) { return pt.x == 0 && pt.y == 0; }))
			return;

		// insert or delete lines
		if (m_insertionDeletionDetectionMode == INSERTION_DELETION_DETECTION_VERTICAL)
		{
			for (auto it = m_lineDiffInfos.crbegin(); it != m_lineDiffInfos.crend(); ++it)
			{
				if (static_cast<int>(rc.top * m_diffBlockSize) <= it->dbegin &&
				    it->dend[srcPane] < static_cast<int>(rc.bottom * m_diffBlockSize))
				{
					int dh = it->dend[srcPane] - it->dend[dstPane];
					if (dh > 0)
					{
						InsertRows(dstPane, it->end[dstPane] + 1, dh);
						for (int i = 0; i < it->end[srcPane] + 1 - it->begin[srcPane]; ++i)
							memcpy(m_imgOrig32[dstPane].scanLine(it->begin[dstPane] + i),
							       m_imgOrig32[srcPane].scanLine(it->begin[srcPane] + i),
							       m_imgOrig32[dstPane].width() * 4);
					}
					else if (dh < 0)
					{
						DeleteRows(dstPane, it->end[dstPane] + 1 + dh, -dh);
					}
				}
			}
		}
		else if (m_insertionDeletionDetectionMode == INSERTION_DELETION_DETECTION_HORIZONTAL)
		{
			for (auto it = m_lineDiffInfos.crbegin(); it != m_lineDiffInfos.crend(); ++it)
			{
				if (static_cast<int>(rc.left * m_diffBlockSize) <= it->dbegin &&
				    it->dend[srcPane] < static_cast<int>(rc.right * m_diffBlockSize))
				{
					int dw = it->dend[srcPane] - it->dend[dstPane];
					if (dw > 0)
					{
						InsertColumns(dstPane, it->end[dstPane] + 1, dw);
						for (unsigned y = 0; y < m_imgOrig32[srcPane].height(); ++y)
						{
							memcpy(m_imgOrig32[dstPane].scanLine(y) + it->begin[dstPane] * 4,
								   m_imgOrig32[srcPane].scanLine(y) + it->begin[srcPane] * 4,
								   (it->end[srcPane] + 1 - it->begin[srcPane]) * 4);
						}
					}
					else if (dw < 0)
					{
						DeleteColumns(dstPane, it->end[dstPane] + 1 + dw, -dw);
					}
				}
			}
		}
	}

private:
	bool m_bRO[3];
	UndoRecords m_undoRecords;
};

