//=============================================================================
//  MuseScore
//  Music Composition & Notation
//
//  Copyright (C) 2002-2011 Werner Schweer
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2
//  as published by the Free Software Foundation and appearing in
//  the file LICENCE.GPL
//=============================================================================

#ifndef __OCTAVEDOT_H__
#define __OCTAVEDOT_H__

#include "element.h"

namespace Ms {

class Note;
class Rest;

//---------------------------------------------------------
//   @@ OctaveDot
//   used in Jianpu, added above or below Jianpu notes, to represent different octaves of each note.
//   one OctaveDot object may draw multi dots.
//---------------------------------------------------------

class OctaveDot : public Element {

   public:
	  OctaveDot(Score* s = 0) : Element(s) { };
      enum DotDirection {TOP, BOTTOM};
      virtual OctaveDot* clone() const override     { return new OctaveDot(*this); }
      virtual ElementType type() const override   { return ElementType::OCTAVE_DOT; }

      virtual void draw(QPainter*) const override;
      // virtual void read(XmlReader&) override;
      virtual void layout() override;
	  virtual qreal mag() const override;

      Note* note() const { return parent()->isNote() ? toNote(parent()) : 0; }
      Rest* rest() const { return parent()->isRest() ? toRest(parent()) : 0; }

	  void setDotCount(int dotcount) { _dotCount = dotcount; }
	  void setDotDirection(DotDirection dotposition) { direction = dotposition; }

	  static const int MAX_OCTAVE_DOTS = 4;
	  static const int OCTAVE_DOT_WIDTH = 10;
	  static const int OCTAVE_DOT_HEIGHT = 10;
	  static const int OCTAVE_DOT_X_SPACE = 3;  // Horizontal Space between octave dots
	  static const int OCTAVE_DOT_Y_SPACE = 3;  // Vertical Space between octave dots
	  static const int OCTAVE_DOT_COL_WIDTH = OCTAVE_DOT_WIDTH + OCTAVE_DOT_X_SPACE;
	  static const int OCTAVE_DOT_ROW_HEIGHT = OCTAVE_DOT_HEIGHT + OCTAVE_DOT_Y_SPACE;
	  // static const int OCTAVE_DOTBOX_WIDTH = MAX_OCTAVE_COLS * OCTAVE_DOT_COL_WIDTH; // For 2x2 dots
	  // static const int OCTAVE_DOTBOX_HEIGHT = MAX_OCTAVE_ROWS * OCTAVE_DOT_ROW_HEIGHT; // For 2x2 dots
	  static const int OCTAVE_DOTBOX_Y_OFFSET = 10;  // Y-offset between octave dot and note number boxes

    private:
      int _dotCount;  /// Octave number of note:  0 (middle octave, octave #4),
                        ///<     negative (lower octaves), positive (upper octaves)
      DotDirection direction;
      };


}     // namespace Ms
#endif

