/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Tristan Porteries.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file KX_TextMaterial.h
 *  \ingroup ketsji
 *  \brief Fake material used for all text objects.
 */

#ifndef __KX_TEXTMATERIAL_H__
#define __KX_TEXTMATERIAL_H__

#include "RAS_IPolygonMaterial.h"

class KX_TextMaterial : public RAS_IPolyMaterial
{
public:
	KX_TextMaterial();
	virtual ~KX_TextMaterial();

	virtual void Activate(RAS_Rasterizer *rasty);
	virtual void Desactivate(RAS_Rasterizer *rasty);
	virtual void ActivateInstancing(RAS_Rasterizer *rasty, void *matrixoffset, void *positionoffset, void *coloroffset, unsigned int stride);
	virtual void DesactivateInstancing();
	virtual void ActivateMeshSlot(RAS_MeshSlot *ms, RAS_Rasterizer *rasty);

	virtual const std::string GetTextureName() const;
	virtual Material *GetBlenderMaterial() const;
	virtual Scene *GetBlenderScene() const;
	virtual SCA_IScene *GetScene() const;
	virtual bool UseInstancing() const;
	virtual void ReleaseMaterial();

	virtual void UpdateIPO(MT_Vector4 rgba, MT_Vector3 specrgb, float hard, float spec, float ref,
						   float emit, float ambient, float alpha, float specalpha);

	virtual const RAS_Rasterizer::AttribLayerList GetAttribLayers(const RAS_MeshObject::LayersInfo& layersInfo) const;

	virtual void OnConstruction();
};

/// Global text material instance pointer used to create or find a material bucket in the bucket manager.
KX_TextMaterial *GetTextMaterial();

#endif  // __KX_TEXTMATERIAL_H__
