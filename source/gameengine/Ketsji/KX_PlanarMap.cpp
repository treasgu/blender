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
* Contributor(s): Ulysse Martin, Tristan Porteries, Martins Upitis.
*
* ***** END GPL LICENSE BLOCK *****
*/

/** \file KX_PlanarMap.cpp
*  \ingroup ketsji
*/

#include "KX_PlanarMap.h"
#include "KX_Camera.h"
#include "KX_PyMath.h"
#include "KX_Globals.h"

#include "RAS_Rasterizer.h"
#include "RAS_Texture.h"

KX_PlanarMap::KX_PlanarMap(EnvMap *env, KX_GameObject *viewpoint)
	:KX_TextureRenderer(env, viewpoint),
	m_normal(0.0f, 0.0f, 1.0f)
{
	m_faces.emplace_back(RAS_Texture::GetTexture2DType());

	switch (env->mode) {
		case ENVMAP_REFLECTION:
		{
			m_type = REFLECTION;
			break;
		}
		case ENVMAP_REFRACTION:
		{
			m_type = REFRACTION;
			break;
		}
	}
}

KX_PlanarMap::~KX_PlanarMap()
{
}

std::string KX_PlanarMap::GetName()
{
	return "KX_PlanarMap";
}

void KX_PlanarMap::ComputeClipPlane(const MT_Vector3& mirrorObjWorldPos, const MT_Matrix3x3& mirrorObjWorldOri)
{
	const MT_Vector3 normal = mirrorObjWorldOri * m_normal;

	m_clipPlane.x = normal.x;
	m_clipPlane.y = normal.y;
	m_clipPlane.z = normal.z;
	m_clipPlane.w = -(m_clipPlane.x * mirrorObjWorldPos.x +
					    m_clipPlane.y * mirrorObjWorldPos.y +
					    m_clipPlane.z * mirrorObjWorldPos.z);
}

void KX_PlanarMap::InvalidateProjectionMatrix()
{
	m_projections.clear();
}

const MT_Matrix4x4& KX_PlanarMap::GetProjectionMatrix(RAS_Rasterizer *rasty, KX_Scene *scene, KX_Camera *sceneCamera,
													  const RAS_Rect& viewport, const RAS_Rect& area)
{
	std::unordered_map<KX_Camera *, MT_Matrix4x4>::const_iterator projectionit = m_projections.find(sceneCamera);
	if (projectionit != m_projections.end()) {
		return projectionit->second;
	}

	MT_Matrix4x4& projection = m_projections[sceneCamera];

	RAS_FrameFrustum frustum;
	const bool orthographic = !sceneCamera->GetCameraData()->m_perspective;

	if (orthographic) {
		RAS_FramingManager::ComputeOrtho(
		    scene->GetFramingType(),
		    area,
		    viewport,
		    sceneCamera->GetScale(),
		    m_clipStart,
		    m_clipEnd,
		    sceneCamera->GetSensorFit(),
		    sceneCamera->GetShiftHorizontal(),
		    sceneCamera->GetShiftVertical(),
		    frustum);
	}
	else {
		RAS_FramingManager::ComputeFrustum(
		    scene->GetFramingType(),
		    area,
		    viewport,
		    sceneCamera->GetLens(),
		    sceneCamera->GetSensorWidth(),
		    sceneCamera->GetSensorHeight(),
		    sceneCamera->GetSensorFit(),
		    sceneCamera->GetShiftHorizontal(),
		    sceneCamera->GetShiftVertical(),
		    m_clipStart,
		    m_clipEnd,
		    frustum);
	}

	if (!sceneCamera->GetViewport()) {
		KX_KetsjiEngine *engine = KX_GetActiveEngine();
		const float camzoom = engine->GetCameraZoom(sceneCamera);

		frustum.x1 *= camzoom;
		frustum.x2 *= camzoom;
		frustum.y1 *= camzoom;
		frustum.y2 *= camzoom;
	}

	if (orthographic) {
		projection = rasty->GetOrthoMatrix(
		    frustum.x1, frustum.x2, frustum.y1, frustum.y2, frustum.camnear, frustum.camfar);
	}
	else {
		const float focallength = sceneCamera->GetFocalLength();

		projection = rasty->GetFrustumMatrix(RAS_Rasterizer::RAS_STEREO_LEFTEYE,
		    frustum.x1, frustum.x2, frustum.y1, frustum.y2, frustum.camnear, frustum.camfar, focallength);
	}

	return projection;
}

void KX_PlanarMap::BeginRenderFace(RAS_Rasterizer *rasty)
{
	KX_TextureRenderer::BeginRenderFace(rasty);

	if (m_type == REFLECTION) {
		rasty->SetInvertFrontFace(true);
		rasty->EnableClipPlane(0, m_clipPlane);
	}
	else {
		rasty->EnableClipPlane(0, -m_clipPlane);
	}
}

void KX_PlanarMap::EndRenderFace(RAS_Rasterizer *rasty)
{
	if (m_type == REFLECTION) {
		rasty->SetInvertFrontFace(false);
	}
	rasty->DisableClipPlane(0);

	KX_TextureRenderer::EndRenderFace(rasty);
}

const MT_Vector3& KX_PlanarMap::GetNormal() const
{
	return m_normal;
}

void KX_PlanarMap::SetNormal(const MT_Vector3& normal)
{
	m_normal = normal.Normalized();
}

bool KX_PlanarMap::SetupCamera(KX_Camera *sceneCamera, KX_Camera *camera)
{
	KX_GameObject *mirror = GetViewpointObject();

	// Compute camera position and orientation.
	const MT_Matrix3x3& mirrorObjWorldOri = mirror->NodeGetWorldOrientation();
	const MT_Vector3& mirrorObjWorldPos = mirror->NodeGetWorldPosition();

	MT_Vector3 cameraWorldPos = sceneCamera->NodeGetWorldPosition();

	// Update clip plane to possible new normal or viewpoint object.
	ComputeClipPlane(mirrorObjWorldPos, mirrorObjWorldOri);

	const float d = m_clipPlane.x * cameraWorldPos.x +
			  m_clipPlane.y * cameraWorldPos.y +
			  m_clipPlane.z * cameraWorldPos.z +
			  m_clipPlane.w;

	// Check if the scene camera is in the right plane side.
	if (d < 0.0) {
		return false;
	}

	const MT_Matrix3x3 mirrorObjWorldOriInverse = mirrorObjWorldOri.inverse();
	MT_Matrix3x3 cameraWorldOri = sceneCamera->NodeGetWorldOrientation();

	static const MT_Matrix3x3 unmir(1.0f, 0.0f, 0.0f,
									0.0f, 1.0f, 0.0f,
									0.0f, 0.0f, -1.0f);

	if (m_type == REFLECTION) {
		// Get vector from mirror to camera in mirror space.
		cameraWorldPos = (cameraWorldPos - mirrorObjWorldPos) * mirrorObjWorldOri;

		cameraWorldPos = mirrorObjWorldPos + cameraWorldPos * unmir * mirrorObjWorldOriInverse;
		cameraWorldOri.transpose();
		cameraWorldOri = cameraWorldOri * mirrorObjWorldOri * unmir * mirrorObjWorldOriInverse;
		cameraWorldOri.transpose();
	}

	// Set render camera position and orientation.
	camera->NodeSetWorldPosition(cameraWorldPos);
	camera->NodeSetGlobalOrientation(cameraWorldOri);

	return true;
}

bool KX_PlanarMap::SetupCameraFace(KX_Camera *camera, unsigned short index)
{
	return true;
}

#ifdef WITH_PYTHON

PyTypeObject KX_PlanarMap::Type = {
	PyVarObject_HEAD_INIT(nullptr, 0)
	"KX_PlanarMap",
	sizeof(PyObjectPlus_Proxy),
	0,
	py_base_dealloc,
	0,
	0,
	0,
	0,
	py_base_repr,
	0, 0, 0, 0, 0, 0, 0, 0, 0,
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	0, 0, 0, 0, 0, 0, 0,
	Methods,
	0,
	0,
	&KX_TextureRenderer::Type,
	0, 0, 0, 0, 0, 0,
	py_base_new
};

PyMethodDef KX_PlanarMap::Methods[] = {
	{nullptr, nullptr} // Sentinel
};

PyAttributeDef KX_PlanarMap::Attributes[] = {
	KX_PYATTRIBUTE_RW_FUNCTION("normal", KX_PlanarMap, pyattr_get_normal, pyattr_set_normal),
	KX_PYATTRIBUTE_NULL // Sentinel
};

PyObject *KX_PlanarMap::pyattr_get_normal(PyObjectPlus *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_PlanarMap *self = static_cast<KX_PlanarMap *>(self_v);
	return PyObjectFrom(self->GetNormal());
}

int KX_PlanarMap::pyattr_set_normal(PyObjectPlus *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_PlanarMap *self = static_cast<KX_PlanarMap *>(self_v);

	MT_Vector3 normal;
	if (!PyVecTo(value, normal)) {
		return PY_SET_ATTR_FAIL;
	}

	self->SetNormal(normal);

	return PY_SET_ATTR_SUCCESS;
}

#endif  // WITH_PYTHON
