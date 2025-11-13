#pragma once

#include <gtk/gtk.h>

#include <memory>
#include <string_view>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <optional>
#include <stdexcept>
#include <forward_list>
#include <cassert>

#include "ScintillaTypes.h"
#include "Geometry.h"
#include "Platform.h"
#include "Position.h"
#include "Style.h"
#include "LineMarker.h"
#include "UniqueString.h"
#include "Indicator.h"
#include "ViewStyle.h"
#include "CharClassify.h"
#include "ILexer.h"
#include "ILoader.h"
#include "Debugging.h"
#include "SplitVector.h"
#include "CellBuffer.h"
#include "CharacterCategoryMap.h"
#include "CaseFolder.h"
#include "Partitioning.h"
#include "RunStyles.h"
#include "Decoration.h"
#include "Document.h"
#include "Selection.h"
#include "PositionCache.h"
#include "ContractionState.h"
#include "EditModel.h"
#include "ScintillaMessages.h"
#include "ScintillaStructures.h"
#include "MarginView.h"
#include "EditView.h"
#include "KeyMap.h"
#include "Editor.h"
#include "AutoComplete.h"
#include "CallTip.h"
#include "ScintillaBase.h"

struct _ScintillaView;

namespace Scintilla::Internal {

class ScintillaGTK final : public ScintillaBase {
	friend struct ::_ScintillaView;

public:
	ScintillaGTK(struct _ScintillaView *widget);
	sptr_t WndProc(Message iMessage, uptr_t wParam, sptr_t lParam) override;

private:
	void ScrollTo(double adj_value);
	void SetVerticalScrollPos() override;
	void SetHorizontalScrollPos() override;
	bool ModifyScrollBars(Sci::Line nMax, Sci::Line nPage) override;

	bool in_measure { false };
	void SetScrollBars() override;
	// Point GetVisibleOriginInMain() const override;
	void Redraw() override;

	void Copy() override;
	bool CanPaste() override;
	void Paste() override;
	void Paste(GdkClipboard *clipboard);
	void ClaimSelection() override;
	void CopyToClipboard(const SelectionText &selectedText) override;

	void DoPaste(const GValue *value) noexcept;
	static void ReadReady(GObject *source_object, GAsyncResult *res, gpointer data) noexcept;

	void NotifyChange() override;
	void NotifyParent(Scintilla::NotificationData scn) override;

	bool haveMouseCapture { false };
	void SetMouseCapture(bool) override;
	bool HaveMouseCapture() override;

	std::string UTF8FromEncoded(std::string_view encoded) const override;
	std::string EncodedFromUTF8(std::string_view utf8) const override;

	sptr_t DefWndProc(Message iMessage, uptr_t wParam, sptr_t lParam) override;
	void CreateCallTipWindow(PRectangle rc) override;
	void AddToPopUp(const char *label, int cmd=0, bool enabled=true) override;

	_ScintillaView *widget() const;

	struct Ticker {
		ScintillaGTK *sc { nullptr };
		guint source_id { 0 };
		~Ticker();
		Ticker() {}
	};
	// We never use TickReason::platform
	Ticker tickers[static_cast<size_t>(TickReason::dwell)+1];
	bool FineTickerRunning(TickReason reason) override;
	void FineTickerStart(TickReason reason, int millis, int tolerance) override;
	void FineTickerCancel(TickReason reason) override;
	static gboolean FineTickerCallback(Ticker *);
};

}
