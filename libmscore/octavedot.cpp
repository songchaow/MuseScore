//=============================================================================
//  MuseScore
//  Music Composition & Notation
//
//  Copyright (C) 2002-2018 Werner Schweer
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2
//  as published by the Free Software Foundation and appearing in
//  the file LICENCE.GPL
//=============================================================================

#include "octavedot.h"
#include "jianpunote.h"
#include "jianpubeam.h"
#include "chord.h"


namespace Ms{
	void OctaveDot::layout()
	{
	// set bbox
	auto noteNumberBox = ((JianpuNote*)parent())->noteNumberBox();
	qreal box_height = (_dotCount > MAX_OCTAVE_DOTS ? MAX_OCTAVE_DOTS : _dotCount)*OCTAVE_DOT_ROW_HEIGHT*mag();
	QRectF bbox;
	bbox.setRect(0, 0, noteNumberBox.width(), box_height);
	setbbox(bbox);
	// set position
	switch (direction)
		{
		
	case TOP:
		setPos(0, -box_height - OCTAVE_DOTBOX_Y_OFFSET * mag());
		break;
	case BOTTOM:

		Chord* chord = ((JianpuNote*)parent())->chord();
		Beam * beam = chord->beam();
		// octave dots must be placed below beam if beam exists
		if (beam) {
			JianpuBeam* jbeam = dynamic_cast<JianpuBeam*>(beam);
			Q_ASSERT(jbeam != nullptr);
			// find the depth under current horizontal position
			qreal xoff = parent()->pageX() - jbeam->pageX() + chord->width() / 2;
			auto ppx = parent()->pageX();
			auto jpx = jbeam->pageX();
			auto cwidth = chord->width() / 2;
			qreal depth;
			//if (jbeam->getBeamSegmentsCache().empty()) {
				// beams' layout() hasn't been called before. Estimate the proper depth according to hook count.
				qreal beamDistance = (JianpuNote::BEAM_HEIGHT + JianpuNote::BEAM_Y_SPACE) * jbeam->mag();
				depth = (chord->beams() > 1 ? chord->beams() - 1 : 0)*beamDistance + noteNumberBox.height();
			
			//else depth = jbeam->getBeamDepth(xoff) + jbeam->pagePos().y() - parent()->pagePos().y();
			setPos(0, depth + OCTAVE_DOTBOX_Y_OFFSET * mag());
		}
		else
			setPos(0, noteNumberBox.height() + OCTAVE_DOTBOX_Y_OFFSET * mag());
		QPointF beam_offset = -pagePos();
		break;
		}
	}

qreal OctaveDot::mag() const
	{
	if (parent() && parent()->type() == ElementType::NOTEHEAD)
		return parent()->mag();
	else return 1.0f;
	}

void OctaveDot::draw(QPainter* painter) const{
	QBrush brush(curColor(), Qt::SolidPattern);
	painter->setBrush(brush);
	painter->setPen(Qt::NoPen);
	qreal x = bbox().width() / 2 - OCTAVE_DOT_WIDTH * mag() / 2, y = 0, yOffset = 0;
	
	if (direction == DotDirection::TOP) {
		// start drawing at bottom of bbox
		y = bbox().height() - OCTAVE_DOT_HEIGHT * mag();
		yOffset = -bbox().height() / _dotCount;
		}
	else if (direction == DotDirection::BOTTOM) {
		// start drawing at top of bbox
		y = 0;
		yOffset = bbox().height() / _dotCount;
		}
	QRectF dotRect(0, 0, OCTAVE_DOT_WIDTH*mag(), OCTAVE_DOT_HEIGHT*mag());
	for (int i = 0; i < _dotCount; i++) {
		dotRect.moveTo(x, y);
		painter->drawEllipse(dotRect);
		y += yOffset;
		}
	}




}