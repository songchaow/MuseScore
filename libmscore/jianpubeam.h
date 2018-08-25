//=============================================================================
//  MuseScore
//  Music Composition & Notation
//
//  Copyright (C) 2002-2017 Werner Schweer
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2
//  as published by the Free Software Foundation and appearing in
//  the file LICENCE.GPL
//=============================================================================

#ifndef __JIANPUBEAM_H__
#define __JIANPUBEAM_H__

#include "beam.h"
#include "element.h"
#include "durationtype.h"
#include "xml.h"

class QPainter;

namespace Ms {

class ChordRest;
class MuseScoreView;
class Chord;
class System;

//---------------------------------------------------------
//   @@ JianpuBeam
//---------------------------------------------------------

class JianpuBeam : public Beam {
      Q_GADGET

   public:
      JianpuBeam(Score* = 0);
      JianpuBeam(const Beam&);
      JianpuBeam(const JianpuBeam&);
      ~JianpuBeam();

      virtual JianpuBeam* clone() const override { return new JianpuBeam(*this); }
      virtual bool isEditable() const override { return false; }

      virtual void read(XmlReader& xml) override;
      virtual void write(XmlWriter& xml) const override;

      virtual void layout() override;
      virtual void draw(QPainter*) const override;
	  virtual qreal mag() const override;

	  // get max depth of beam in current horizontal position
	  // x: position relative to beam's position
	  qreal getBeamDepth(qreal x);

	  const QVector<QLineF*>& getBeamSegmentsCache() const { return beamSegmentsCache; }

	private:
	  // Member `beamSegments` will be cleared in `createBeams()` method before Chord's `layout()` is called.
	  // But Octavedot's `layout()` needs `beamSegments` to locate dots vertical location.
	  // So we use this member as a backup.
	  QVector<QLineF*> beamSegmentsCache;
      };

} // namespace Ms

#endif

