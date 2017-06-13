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

/** \file gameengine/Ketsji/KX_TextMaterial.cpp
 *  \ingroup ketsji
 */

#include "KX_TextMaterial.h"

#include "DNA_material_types.h"

static KX_TextMaterial textMaterial = KX_TextMaterial();

KX_TextMaterial *GetTextMaterial()
{
	return &textMaterial;
}

KX_TextMaterial::KX_TextMaterial()
	:RAS_IPolyMaterial("__TextMaterial__", nullptr)
{
	m_rasMode |= (RAS_ALPHA | RAS_TEXT);
	m_flag |= RAS_BLENDERGLSL;
	m_alphablend = GEMAT_ALPHA;
}

KX_TextMaterial::~KX_TextMaterial()
{
}

void KX_TextMaterial::Activate(RAS_Rasterizer *rasty)
{
}

void KX_TextMaterial::Desactivate(RAS_Rasterizer *rasty)
{
}

void KX_TextMaterial::ActivateInstancing(RAS_Rasterizer *rasty, void *matrixoffset, void *positionoffset, void *coloroffset, unsigned int stride)
{
}

void KX_TextMaterial::DesactivateInstancing()
{
}

void KX_TextMaterial::ActivateMeshSlot(RAS_MeshSlot *ms, RAS_Rasterizer *rasty)
{
}

const std::string KX_TextMaterial::GetTextureName() const
{
	return "";
}

Material *KX_TextMaterial::GetBlenderMaterial() const
{
	return nullptr;
}

Scene *KX_TextMaterial::GetBlenderScene() const
{
	return nullptr;
}

SCA_IScene *KX_TextMaterial::GetScene() const
{
	return nullptr;
}

bool KX_TextMaterial::UseInstancing() const
{
	return false;
}

void KX_TextMaterial::ReleaseMaterial()
{
}

void KX_TextMaterial::UpdateIPO(MT_Vector4 rgba, MT_Vector3 specrgb, float hard, float spec, float ref,
			   float emit, float ambient, float alpha, float specalpha)
{
}

const RAS_Rasterizer::AttribLayerList KX_TextMaterial::GetAttribLayers(const RAS_MeshObject::LayersInfo& layersInfo) const
{
	static const RAS_Rasterizer::AttribLayerList attribLayers;
	return attribLayers;
}

void KX_TextMaterial::OnConstruction()
{
}
