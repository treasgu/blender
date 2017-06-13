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

/** \file gameengine/Ketsji/KX_BatchGroup.cpp
*  \ingroup ketsji
*/

#include "KX_BatchGroup.h"
#include "KX_GameObject.h"
#include "KX_Globals.h"
#include "RAS_MeshUser.h"

#include "CM_Message.h"

KX_BatchGroup::KX_BatchGroup()
{
	m_objects = new CListValue<KX_GameObject>();
	// The objects are not owned by the batching group, so not released on list releasing.
	m_objects->SetReleaseOnDestruct(false);
}

KX_BatchGroup::~KX_BatchGroup()
{
	m_objects->Release();
}

std::string KX_BatchGroup::GetName()
{
	return "KX_BatchGroup";
}

CListValue<KX_GameObject> *KX_BatchGroup::GetObjects() const
{
	return m_objects;
}

void KX_BatchGroup::MergeObjects(const std::vector<KX_GameObject *>& objects)
{
	for (std::vector<KX_GameObject *>::const_iterator it = objects.begin(), end = objects.end(); it != end; ++it) {
		KX_GameObject *gameobj = *it;
		RAS_MeshUser *meshUser = gameobj->GetMeshUser();

		if (!meshUser) {
			CM_Error("object \"" << gameobj->GetName() << "\" doesn't contain a mesh");
			continue;
		}

		if (meshUser->GetBatchGroup()) {
			CM_Error("object \"" << gameobj->GetName() << "\" already used in a batch group");
			continue;
		}

		MT_Transform trans(gameobj->NodeGetWorldPosition(), gameobj->NodeGetWorldOrientation());
		const MT_Vector3& scale = gameobj->NodeGetWorldScaling();
		trans.scale(scale.x, scale.y, scale.z);

		if (MergeMeshUser(meshUser, trans.toMatrix())) {
			m_objects->Add(gameobj);
		}
		else {
			CM_Error("failed merge object \"" << gameobj->GetName() << "\"");
		}
	}
}

void KX_BatchGroup::SplitObjects(const std::vector<KX_GameObject *>& objects)
{
	// Add a fake mesh user to avoid free the batch group while running the function.
	AddMeshUser();

	for (std::vector<KX_GameObject *>::const_iterator it = objects.begin(), end = objects.end(); it != end; ++it) {
		KX_GameObject *gameobj = *it;
		RAS_MeshUser *meshUser = gameobj->GetMeshUser();

		if (!meshUser) {
			CM_Error("object \"" << gameobj->GetName() << "\" doesn't contain a mesh");
			continue;
		}

		if (SplitMeshUser(meshUser)) {
			m_objects->RemoveValue(gameobj);
		}
		else {
			CM_Error("failed split object \"" << gameobj->GetName() << "\"");
		}
	}

	RemoveMeshUser();
}

#ifdef WITH_PYTHON

static PyObject *py_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	PyObject *pylist;

	if (!PyArg_ParseTuple(args,"O:KX_BatchGroup", &pylist)) {
		return nullptr;
	}

	if (!PyList_Check(pylist)) {
		PyErr_SetString(PyExc_SystemError, "KX_BatchGroup(objects): expected a list");
		return nullptr;
	}

	std::vector<KX_GameObject *> objects;

	for (unsigned short i = 0; i < PyList_GET_SIZE(pylist); ++i) {
		PyObject *pyobj = PyList_GET_ITEM(pylist, i);
		KX_GameObject *gameobj;

		if (!ConvertPythonToGameObject(KX_GetActiveScene()->GetLogicManager(), pyobj, &gameobj, false, "KX_BatchGroup(objects)")) {
			return nullptr;
		}

		objects.push_back(gameobj);
	}

	KX_BatchGroup *batchGroup = new KX_BatchGroup();
	batchGroup->MergeObjects(objects);
	if (batchGroup->GetObjects()->GetCount() == 0) {
		PyErr_SetString(PyExc_SystemError, "KX_BatchGroup(objects): none objects were merged.");
		delete batchGroup;
		return nullptr;
	}

	return batchGroup->GetProxy();
}

PyTypeObject KX_BatchGroup::Type = {
	PyVarObject_HEAD_INIT(nullptr, 0)
	"KX_BatchGroup",
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
	&PyObjectPlus::Type,
	0, 0, 0, 0, 0, 0,
	py_new
};

PyMethodDef KX_BatchGroup::Methods[] = {
	KX_PYMETHODTABLE(KX_BatchGroup, merge),
	KX_PYMETHODTABLE(KX_BatchGroup, split),
	KX_PYMETHODTABLE(KX_BatchGroup, destruct),
	{nullptr, nullptr} // Sentinel
};

PyAttributeDef KX_BatchGroup::Attributes[] = {
	KX_PYATTRIBUTE_RO_FUNCTION("objects", KX_BatchGroup, pyattr_get_objects),
	KX_PYATTRIBUTE_NULL // Sentinel
};

PyObject *KX_BatchGroup::pyattr_get_objects(PyObjectPlus *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_BatchGroup *self = static_cast<KX_BatchGroup *>(self_v);
	return self->GetObjects()->GetProxy();
}

KX_PYMETHODDEF_DOC(KX_BatchGroup, merge, "merge(objects)")
{
	PyObject *pylist;
	if (!PyArg_ParseTuple(args, "O:merge", &pylist)) {
		return nullptr;
	}

	if (!PyList_Check(pylist)) {
		PyErr_SetString(PyExc_SystemError, "batch.merge(objects): expected a list");
		return nullptr;
	}

	std::vector<KX_GameObject *> objects;

	for (unsigned short i = 0; i < PyList_GET_SIZE(pylist); ++i) {
		PyObject *pyobj = PyList_GET_ITEM(pylist, i);
		KX_GameObject *gameobj;

		if (!ConvertPythonToGameObject(KX_GetActiveScene()->GetLogicManager(), pyobj, &gameobj, false, "batch.merge(objects)")) {
			return nullptr;
		}

		objects.push_back(gameobj);
	}

	MergeObjects(objects);

	Py_RETURN_NONE;;
}

KX_PYMETHODDEF_DOC(KX_BatchGroup, split, "split(objects)")
{
	PyObject *pylist;
	if (!PyArg_ParseTuple(args, "O:split", &pylist)) {
		return nullptr;
	}

	if (!PyList_Check(pylist)) {
		PyErr_SetString(PyExc_SystemError, "batch.split(objects): expected a list");
		return nullptr;
	}

	std::vector<KX_GameObject *> objects;

	for (unsigned short i = 0; i < PyList_GET_SIZE(pylist); ++i) {
		PyObject *pyobj = PyList_GET_ITEM(pylist, i);
		KX_GameObject *gameobj;

		if (!ConvertPythonToGameObject(KX_GetActiveScene()->GetLogicManager(), pyobj, &gameobj, false, "batch.split(objects)")) {
			return nullptr;
		}

		objects.push_back(gameobj);
	}

	SplitObjects(objects);

	Py_RETURN_NONE;;
}

KX_PYMETHODDEF_DOC(KX_BatchGroup, destruct, "destruct()")
{
	Destruct();

	Py_RETURN_NONE;;
}

#endif  // WITH_PYTHON
