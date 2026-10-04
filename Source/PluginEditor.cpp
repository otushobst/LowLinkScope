#include "PluginEditor.h"

#include <cmath>

//==============================================================================
LowLinkLookAndFeel::LowLinkLookAndFeel()
{
    using namespace juce;
    setColour (Label::textColourId, Colours_::text);
    setColour (ComboBox::backgroundColourId, Colours_::bg);
    setColour (ComboBox::outlineColourId, Colours_::panelEdge);
    setColour (ComboBox::textColourId, Colours_::text);
    setColour (ComboBox::arrowColourId, Colours_::self);
    setColour (PopupMenu::backgroundColourId, Colours_::panel);
    setColour (PopupMenu::textColourId, Colours_::text);
    setColour (PopupMenu::highlightedBackgroundColourId, Colours_::self.withAlpha (0.25f));
    setColour (PopupMenu::highlightedTextColourId, Colours_::text);
    setColour (TextEditor::backgroundColourId, Colours_::bg);
    setColour (TextEditor::outlineColourId, Colours_::panelEdge);
    setColour (TextEditor::focusedOutlineColourId, Colours_::self);
    setColour (TextEditor::textColourId, Colours_::text);
    setColour (TextButton::buttonColourId, Colours_::bg);
    setColour (TextButton::buttonOnColourId, Colours_::self);
    setColour (TextButton::textColourOffId, Colours_::text);
    setColour (TextButton::textColourOnId, Colours_::bg);
    setColour (Slider::textBoxTextColourId, Colours_::dim);
    setColour (Slider::textBoxOutlineColourId, Colours_::bg.withAlpha (0.0f));
}

void LowLinkLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                           float startAngle, float endAngle, juce::Slider& s)
{
    auto r = juce::Rectangle<float> (float (x), float (y), float (w), float (h)).reduced (4.0f);
    const float radius = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;
    const auto c = r.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);

    juce::Path track, value;
    track.addCentredArc (c.x, c.y, radius, radius, 0.0f, startAngle, endAngle, true);
    value.addCentredArc (c.x, c.y, radius, radius, 0.0f, startAngle, angle, true);
    g.setColour (Colours_::panelEdge);
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (accent);
    g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const auto tip = c.getPointOnCircumference (radius * 0.6f, angle);
    g.drawLine (c.x, c.y, tip.x, tip.y, 2.0f);
}

void LowLinkLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                           float, float, juce::Slider::SliderStyle, juce::Slider& s)
{
    auto r = juce::Rectangle<float> (float (x), float (y), float (w), float (h)).withSizeKeepingCentre (float (w), 8.0f);
    g.setColour (Colours_::bg);
    g.fillRoundedRectangle (r, 3.0f);
    const auto accent = s.findColour (juce::Slider::trackColourId);

    if (s.getName() == "position")
    {
        // Show the visible window as a bar
        const float frac = (float) s.getProperties().getWithDefault ("visibleFrac", 1.0f);
        const float bw = juce::jmax (8.0f, r.getWidth() * frac);
        const float bx = r.getX() + (float) s.getValue() * (r.getWidth() - bw);
        g.setColour (accent);
        g.fillRoundedRectangle ({ bx, r.getY(), bw, r.getHeight() }, 3.0f);
        return;
    }
    g.setColour (accent);
    g.fillRoundedRectangle (r.withRight (pos), 3.0f);
}

void LowLinkLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                               bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    auto fill = on ? b.findColour (juce::TextButton::buttonOnColourId) : Colours_::bg;
    if (highlighted && ! on)
        fill = fill.brighter (0.08f);
    if (down)
        fill = fill.darker (0.15f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? fill : Colours_::panelEdge);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

//==============================================================================
namespace
{
    void setupLabel (juce::Label& l, const juce::String& text)
    {
        l.setText (text, juce::dontSendNotification);
        l.setFont (juce::FontOptions (11.0f));
        l.setColour (juce::Label::textColourId, Colours_::dim);
        l.setJustificationType (juce::Justification::centredLeft);
    }

    void setupRotary (juce::Slider& s, juce::Colour accent)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setColour (juce::Slider::rotarySliderFillColourId, accent);
        s.setPopupDisplayEnabled (true, true, nullptr);
    }

    void setupBar (juce::Slider& s, const juce::String& name)
    {
        s.setName (name);
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setColour (juce::Slider::trackColourId, Colours_::self);
    }
}

LowLinkEditor::LowLinkEditor (LowLinkProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    addAndMakeVisible (scope);

    titleLabel.setText ("LOW LINK SCOPE", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, Colours_::self);
    addAndMakeVisible (titleLabel);

    setupLabel (nameLabel, "THIS TRACK");
    setupLabel (linkLabel, "COMPARE WITH");
    setupLabel (sizeLabel, "SIZE");
    setupLabel (showLabel, "SHOW");
    setupLabel (lpLabel, "LOW-PASS");
    setupLabel (ampLabel, "AMP");
    setupLabel (selfLevelLabel, "THIS");
    setupLabel (linkLevelLabel, "LINKED");
    setupLabel (zoomLabel, "ZOOM");
    setupLabel (posLabel, "POSITION");
    for (auto* l : { &nameLabel, &linkLabel, &sizeLabel, &showLabel, &lpLabel, &ampLabel,
                     &selfLevelLabel, &linkLevelLabel, &zoomLabel, &posLabel })
        addAndMakeVisible (*l);
    ampLabel.setJustificationType (juce::Justification::centred);
    selfLevelLabel.setJustificationType (juce::Justification::centred);
    linkLevelLabel.setJustificationType (juce::Justification::centred);

    nameEditor.setFont (juce::FontOptions (13.0f));
    // Empty = follow the DAW track name (shown as placeholder); typed text = custom name.
    nameEditor.setTextToShowWhenEmpty (proc.getDisplayName(), Colours_::dim);
    nameEditor.setText (proc.getCustomName(), juce::dontSendNotification);
    nameEditor.onReturnKey = [this] { proc.setCustomName (nameEditor.getText()); nameEditor.giveAwayKeyboardFocus(); };
    nameEditor.onFocusLost = [this] { proc.setCustomName (nameEditor.getText()); };
    addAndMakeVisible (nameEditor);

    linkBox.setTextWhenNothingSelected ("Select track...");
    linkBox.setTextWhenNoChoicesAvailable ("No other instance");
    linkBox.onChange = [this]
    {
        const int id = linkBox.getSelectedId();
        if (id <= 1)
            proc.setLinkTargetName ({});
        else
            proc.setLinkTargetName (linkBox.getItemText (linkBox.indexOfItemId (id)));
        lastKey = {};
    };
    addAndMakeVisible (linkBox);

    sizeBox.addItemList (kSizeNames, 1);
    lpBox.addItemList (kLowpassNames, 1);
    addAndMakeVisible (sizeBox);
    addAndMakeVisible (lpBox);
    sizeAtt = std::make_unique<ComboAttachment> (proc.apvts, ParamIDs::size, sizeBox);
    lpAtt = std::make_unique<ComboAttachment> (proc.apvts, ParamIDs::lowpass, lpBox);

    for (int i = 0; i < 3; ++i)
    {
        auto& b = chanButtons[i];
        b.setButtonText (kChannelNames[i]);
        b.setClickingTogglesState (false);
        b.onClick = [this, i] { setChannel (i); };
        addAndMakeVisible (b);
    }
    chanButtons[0].setColour (juce::TextButton::buttonOnColourId, Colours_::self);
    chanButtons[1].setColour (juce::TextButton::buttonOnColourId, Colours_::text);
    chanButtons[2].setColour (juce::TextButton::buttonOnColourId, Colours_::link);

    mixButton.setClickingTogglesState (true);
    mixButton.setColour (juce::TextButton::buttonOnColourId, Colours_::sum);
    addAndMakeVisible (mixButton);
    mixAtt = std::make_unique<ButtonAttachment> (proc.apvts, ParamIDs::mix, mixButton);

    freezeButton.setClickingTogglesState (true);
    freezeButton.setColour (juce::TextButton::buttonOnColourId, Colours_::link);
    freezeButton.onClick = [this] { if (! freezeButton.getToggleState()) lastKey = {}; };
    addAndMakeVisible (freezeButton);

    setupRotary (ampSlider, Colours_::text);
    setupRotary (selfLevelSlider, Colours_::self);
    setupRotary (linkLevelSlider, Colours_::link);
    for (auto* s : { &ampSlider, &selfLevelSlider, &linkLevelSlider })
        addAndMakeVisible (*s);
    ampAtt = std::make_unique<SliderAttachment> (proc.apvts, ParamIDs::amp, ampSlider);
    selfAtt = std::make_unique<SliderAttachment> (proc.apvts, ParamIDs::selfLevel, selfLevelSlider);
    linkAtt = std::make_unique<SliderAttachment> (proc.apvts, ParamIDs::linkLevel, linkLevelSlider);

    setupBar (zoomSlider, "zoom");
    setupBar (posSlider, "position");
    addAndMakeVisible (zoomSlider);
    addAndMakeVisible (posSlider);
    zoomAtt = std::make_unique<SliderAttachment> (proc.apvts, ParamIDs::zoom, zoomSlider);
    posAtt = std::make_unique<SliderAttachment> (proc.apvts, ParamIDs::position, posSlider);

    setResizable (true, true);
    setResizeLimits (720, 340, 2600, 1500);
    setSize (960, 420);

    refreshLinkList();
    syncChannelButtons();
    startTimerHz (30);
}

LowLinkEditor::~LowLinkEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

int LowLinkEditor::choiceIndex (const char* id) const
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
        return p->getIndex();
    return 0;
}

void LowLinkEditor::setChannel (int index)
{
    if (auto* p = proc.apvts.getParameter (ParamIDs::channel))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (float (index)));
        p->endChangeGesture();
    }
    syncChannelButtons();
}

void LowLinkEditor::syncChannelButtons()
{
    const int c = choiceIndex (ParamIDs::channel);
    for (int i = 0; i < 3; ++i)
        chanButtons[i].setToggleState (i == c, juce::dontSendNotification);
}

ScopeView::Settings LowLinkEditor::currentSettings() const
{
    ScopeView::Settings s;
    s.channel = choiceIndex (ParamIDs::channel);
    s.mix = proc.apvts.getRawParameterValue (ParamIDs::mix)->load() > 0.5f;
    s.amp = proc.apvts.getRawParameterValue (ParamIDs::amp)->load();
    s.selfLevel = proc.apvts.getRawParameterValue (ParamIDs::selfLevel)->load();
    s.linkLevel = proc.apvts.getRawParameterValue (ParamIDs::linkLevel)->load();
    s.zoom = proc.apvts.getRawParameterValue (ParamIDs::zoom)->load();
    s.position = proc.apvts.getRawParameterValue (ParamIDs::position)->load();
    s.frozen = freezeButton.getToggleState();
    return s;
}

void LowLinkEditor::refreshLinkList()
{
    auto& bus = proc.getBus();
    std::vector<lowlink::SlotInfo> others;
    for (auto& info : bus.listAlive())
        if (info.index != proc.getSlot())
            others.push_back (info);

    bool changed = others.size() != linkCandidates.size();
    for (size_t i = 0; ! changed && i < others.size(); ++i)
        changed = others[i].index != linkCandidates[i].index || others[i].name != linkCandidates[i].name;

    const auto target = proc.getLinkTargetName();
    if (changed)
    {
        linkCandidates = others;
        linkBox.clear (juce::dontSendNotification);
        linkBox.addItem ("(none)", 1);
        int selectId = 1;
        for (size_t i = 0; i < linkCandidates.size(); ++i)
        {
            const auto name = juce::String (linkCandidates[i].name);
            const int id = int (i) + 2;
            linkBox.addItem (name, id);
            if (name == target && selectId == 1)
                selectId = id;
        }
        if (target.isNotEmpty() && selectId == 1)
        {
            // Remembered target not running (yet) - keep showing its name.
            linkBox.addItem (target + " (offline)", 999);
            selectId = 999;
        }
        linkBox.setSelectedId (target.isEmpty() ? 1 : selectId, juce::dontSendNotification);
    }

    if (! nameEditor.hasKeyboardFocus (true))
        nameEditor.setTextToShowWhenEmpty (proc.getDisplayName(), Colours_::dim);
}

void LowLinkEditor::timerCallback()
{
    if (++listCounter >= 15)
    {
        listCounter = 0;
        refreshLinkList();
    }
    syncChannelButtons();

    auto s = currentSettings();
    posSlider.getProperties().set ("visibleFrac", 1.0f / juce::jmax (1.0f, s.zoom));
    posSlider.repaint();
    scope.setSettings (s);

    if (! s.frozen)
        acquire();
}

void LowLinkEditor::acquire()
{
    auto& bus = proc.getBus();
    if (! bus.isOpen())
    {
        auto f = std::make_shared<ScopeFrame>();
        f->status = "Link unavailable: " + juce::String (bus.getError());
        scope.setFrame (f);
        return;
    }

    lowlink::SlotInfo self;
    if (! bus.getInfo (proc.getSlot(), self) || self.sampleRate <= 0.0)
        return; // not processing yet

    // Resolve link target by name
    const auto targetName = proc.getLinkTargetName();
    lowlink::SlotInfo other;
    bool haveOther = false;
    if (targetName.isNotEmpty())
        for (auto& info : bus.listAlive())
            if (info.index != proc.getSlot() && juce::String (info.name) == targetName)
            {
                other = info;
                haveOther = true;
                break;
            }

    const int sizeIdx = juce::jlimit (0, 4, choiceIndex (ParamIDs::size));
    const int lpIdx = juce::jlimit (0, 4, choiceIndex (ParamIDs::lowpass));
    const double sizeBeats = kSizeBeats[sizeIdx];
    const double lpHz = kLowpassHz[lpIdx];

    const double sr = self.sampleRate;
    const double bpm = juce::jlimit (20.0, 400.0, self.bpm > 0.0 ? self.bpm : 120.0);
    const double spb = sr * 60.0 / bpm;
    const int maxLen = static_cast<int> (lowlink::kRingSize - 65536);
    const int winLen = juce::jlimit (64, maxLen, static_cast<int> (std::llround (sizeBeats * spb)));

    const bool selfTimeline = self.timeMode == lowlink::timeline && self.playing;
    const bool otherTimeline = haveOther && other.timeMode == lowlink::timeline && other.playing;
    const bool timeline = selfTimeline && (! haveOther || otherTimeline);

    juce::String status;
    int64_t startA = 0, startB = 0;
    Key key;
    key.size = sizeBeats;
    key.lp = lpIdx;
    key.target = haveOther ? other.index : -1;
    key.timeline = timeline;

    if (timeline)
    {
        int64_t endAvail = self.endPos;
        if (haveOther)
        {
            // If the other instance stopped processing (e.g. suspended on silence),
            // don't wait for it - its missing samples just read as silence.
            const bool otherStalled = (self.endPos - other.endPos) > static_cast<int64_t> (sr * 0.5);
            if (! otherStalled)
                endAvail = juce::jmin (self.endPos, other.endPos);
            else
                status = "Linked track not processing";
        }
        const double ppqAtEnd = self.endPpq - double (self.endPos - endAvail) / spb;
        const double k = std::floor (ppqAtEnd / sizeBeats + 1.0e-9);
        const double winStartPpq = (k - 1.0) * sizeBeats;
        startA = std::llround (double (self.endPos) - (self.endPpq - winStartPpq) * spb);
        startB = startA;
        if (startA < 0)
            return;
        key.start = startA;
        if (key == lastKey)
            return;
    }
    else
    {
        status = "Transport stopped - approximate";
        if (lastKey.timeline == false && lastKey.size == sizeBeats && lastKey.lp == lpIdx
            && lastKey.target == key.target && self.endPos - lastFreeRunCapture < winLen)
            return;
        lastFreeRunCapture = self.endPos;
        startA = self.endPos - winLen;
        startB = haveOther ? other.endPos - winLen : 0;
        key.start = startA;
    }
    lastKey = key;

    auto f = std::make_shared<ScopeFrame>();
    f->sampleRate = sr;
    f->sizeBeats = sizeBeats;
    f->nameA = proc.getDisplayName();
    f->nameB = haveOther ? juce::String (other.name) : (targetName.isNotEmpty() ? targetName + " (offline)" : juce::String());
    f->status = status;
    f->a.resize (size_t (winLen));
    bus.readRange (proc.getSlot(), startA, winLen, f->a.data());
    f->hasA = true;

    if (haveOther)
    {
        if (std::abs (other.sampleRate - sr) > 0.5)
            f->status = "Sample rate mismatch";
        f->b.resize (size_t (winLen));
        f->hasB = bus.readRange (other.index, startB, winLen, f->b.data()) > 0;
    }

    if (lpHz > 0.0)
    {
        lowlink::zeroPhaseLowpass (f->a.data(), winLen, sr, lpHz);
        if (f->hasB)
            lowlink::zeroPhaseLowpass (f->b.data(), winLen, sr, lpHz);
    }

    if (f->hasB)
    {
        // Analyse the low end even when the display is full-range.
        const double band = lpHz > 0.0 ? lpHz : 250.0;
        if (lpHz > 0.0)
            f->analysis = lowlink::analyse (f->a.data(), f->b.data(), winLen, sr, band);
        else
        {
            tmpA = f->a;
            tmpB = f->b;
            lowlink::zeroPhaseLowpass (tmpA.data(), winLen, sr, band);
            lowlink::zeroPhaseLowpass (tmpB.data(), winLen, sr, band);
            f->analysis = lowlink::analyse (tmpA.data(), tmpB.data(), winLen, sr, band);
        }
    }

    scope.setFrame (f);
    repaint (statsArea);
}

void LowLinkEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours_::panel);

    // Right panel separator
    g.setColour (Colours_::panelEdge);
    g.drawVerticalLine (getWidth() - 238,10.0f, float (getHeight() - 10));

    // Stats bar
    auto r = statsArea.toFloat();
    g.setColour (Colours_::bg);
    g.fillRoundedRectangle (r, 6.0f);

    const auto frame = scope.getFrame();
    auto inner = r.reduced (12.0f, 6.0f);
    g.setFont (juce::FontOptions (12.0f));

    if (frame == nullptr || ! frame->hasB || ! frame->analysis.valid)
    {
        g.setColour (Colours_::dim);
        juce::String msg = "Pick a track in COMPARE WITH to analyse kick vs bass.";
        if (frame != nullptr && frame->hasB)
            msg = "Waiting for signal on both tracks...";
        g.drawText (msg, inner, juce::Justification::centredLeft);
        return;
    }

    const auto& an = frame->analysis;

    // 1) Sum loss
    auto col1 = inner.removeFromLeft (170.0f);
    g.setColour (Colours_::dim);
    g.drawText ("SUM vs SEPARATE", col1.removeFromTop (col1.getHeight() * 0.45f), juce::Justification::bottomLeft);
    const auto lossCol = an.lossDb < -1.0 ? Colours_::conflict : (an.lossDb > 1.0 ? Colours_::good : Colours_::text);
    g.setColour (lossCol);
    g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    g.drawText ((an.lossDb >= 0 ? "+" : "") + juce::String (an.lossDb, 1) + " dB", col1, juce::Justification::topLeft);

    // 2) Phase meter
    g.setFont (juce::FontOptions (12.0f));
    auto col2 = inner.removeFromLeft (200.0f);
    g.setColour (Colours_::dim);
    g.drawText ("PHASE  " + juce::String (an.correlation, 2), col2.removeFromTop (col2.getHeight() * 0.45f),
                juce::Justification::bottomLeft);
    auto bar = col2.withHeight (8.0f).translated (0.0f, 4.0f).withWidth (180.0f);
    g.setColour (Colours_::panelEdge);
    g.fillRoundedRectangle (bar, 3.0f);
    const float mid = bar.getCentreX();
    const float px = mid + float (an.correlation) * bar.getWidth() * 0.5f;
    g.setColour (an.correlation < 0.0 ? Colours_::conflict : Colours_::good);
    g.fillRoundedRectangle (juce::Rectangle<float>::leftTopRightBottom (juce::jmin (mid, px), bar.getY(),
                                                                         juce::jmax (mid, px), bar.getBottom()), 3.0f);
    g.setColour (Colours_::dim);
    g.drawVerticalLine (static_cast<int> (mid), bar.getY() - 2.0f, bar.getBottom() + 2.0f);

    // 3) Suggestion
    auto col3 = inner;
    g.setColour (Colours_::dim);
    g.drawText ("SUGGESTION", col3.removeFromTop (col3.getHeight() * 0.45f), juce::Justification::bottomLeft);
    juce::String sug;
    if (an.hasSuggestion)
    {
        const auto who = frame->nameB.isNotEmpty() ? frame->nameB : juce::String ("linked track");
        juce::StringArray parts;
        if (std::abs (an.lagMs) > 0.05)
            parts.add ((an.lagMs > 0 ? "delay " : "advance ") + who + " by " + juce::String (std::abs (an.lagMs), 2) + " ms");
        if (an.flip)
            parts.add ((parts.isEmpty() ? "flip polarity of " + who : juce::String ("flip polarity")));
        sug = parts.joinIntoString (" + ") + "  (+" + juce::String (an.gainDb, 1) + " dB)";
        g.setColour (Colours_::text);
    }
    else
    {
        sug = "Alignment looks good";
        g.setColour (Colours_::good);
    }
    g.drawText (sug, col3, juce::Justification::topLeft, true);
}

void LowLinkEditor::resized()
{
    auto area = getLocalBounds().reduced (10);

    // Right panel
    auto panel = area.removeFromRight (220);
    area.removeFromRight (16);

    titleLabel.setBounds (panel.removeFromTop (24));
    panel.removeFromTop (4);

    auto row = [&panel] (int h) { auto r = panel.removeFromTop (h); panel.removeFromTop (4); return r; };

    nameLabel.setBounds (row (14));
    nameEditor.setBounds (row (24));
    linkLabel.setBounds (row (14));
    linkBox.setBounds (row (24));

    {
        auto r = row (14);
        sizeLabel.setBounds (r.removeFromLeft (r.getWidth() / 2));
        lpLabel.setBounds (r);
    }
    {
        auto r = row (24);
        sizeBox.setBounds (r.removeFromLeft (r.getWidth() / 2 - 3));
        r.removeFromLeft (6);
        lpBox.setBounds (r);
    }

    showLabel.setBounds (row (14));
    {
        auto r = row (24);
        const int w = (r.getWidth() - 3 * 4) / 4;
        for (auto& b : chanButtons)
        {
            b.setBounds (r.removeFromLeft (w));
            r.removeFromLeft (4);
        }
        mixButton.setBounds (r);
    }

    panel.removeFromTop (2);
    {
        auto r = row (54);
        const int w = r.getWidth() / 3;
        auto c1 = r.removeFromLeft (w), c2 = r.removeFromLeft (w), c3 = r;
        ampLabel.setBounds (c1.removeFromBottom (14));
        selfLevelLabel.setBounds (c2.removeFromBottom (14));
        linkLevelLabel.setBounds (c3.removeFromBottom (14));
        ampSlider.setBounds (c1);
        selfLevelSlider.setBounds (c2);
        linkLevelSlider.setBounds (c3);
    }
    freezeButton.setBounds (panel.removeFromTop (26));

    // Left: scope, zoom/position, stats
    statsArea = area.removeFromBottom (54);
    area.removeFromBottom (8);
    {
        auto r = area.removeFromBottom (18);
        posLabel.setBounds (r.removeFromLeft (64));
        posSlider.setBounds (r);
    }
    {
        auto r = area.removeFromBottom (18);
        zoomLabel.setBounds (r.removeFromLeft (64));
        zoomSlider.setBounds (r);
    }
    area.removeFromBottom (6);
    scope.setBounds (area);
}
