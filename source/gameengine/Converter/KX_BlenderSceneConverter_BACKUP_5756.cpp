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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gameengine/Converter/KX_BlenderSceneConverter.cpp
 *  \ingroup bgeconv
 */

#ifdef _MSC_VER
#  pragma warning (disable:4786)  // suppress stl-MSVC debug info warning
#endif

#include "KX_Scene.h"
#include "KX_GameObject.h"
#include "KX_IpoConvert.h"
#include "RAS_MeshObject.h"
#include "RAS_IPolygonMaterial.h"
#include "KX_PhysicsEngineEnums.h"
#include "PHY_IPhysicsEnvironment.h"
#include "KX_KetsjiEngine.h"
#include "KX_PythonInit.h" // So we can handle adding new text datablocks for Python to import
#include "BL_ActionActuator.h"

#include "LA_SystemCommandLine.h"

#include "DummyPhysicsEnvironment.h"


#ifdef WITH_BULLET
#include "CcdPhysicsEnvironment.h"
#endif

#include "KX_LibLoadStatus.h"
#include "KX_BlenderScalarInterpolator.h"
#include "BL_BlenderDataConversion.h"
#include "KX_WorldInfo.h"
#include "EXP_StringValue.h"

// This little block needed for linking to Blender...
#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#ifdef WITH_PYTHON
#  include "Texture.h" // For FreeAllTextures. Must be included after BLI_winstuff.h.
#endif  // WITH_PYTHON


// This list includes only data type definitions
#include "DNA_scene_types.h"
#include "DNA_world_types.h"
#include "BKE_main.h"
#include "BKE_fcurve.h"

#include "BLI_math.h"

extern "C" {
#  include "DNA_object_types.h"
#  include "DNA_curve_types.h"
#  include "DNA_mesh_types.h"
#  include "DNA_material_types.h"
#  include "BLI_blenlib.h"
#  include "MEM_guardedalloc.h"
#  include "BKE_global.h"
#  include "BKE_animsys.h"
#  include "BKE_library.h"
#  include "BKE_material.h" // BKE_material_copy
#  include "BKE_mesh.h" // BKE_mesh_copy
#  include "DNA_space_types.h"
#  include "DNA_anim_types.h"
#  include "DNA_action_types.h"
#  include "RNA_define.h"
#  include "../../blender/editors/include/ED_keyframing.h"
}

// Only for dynamic loading and merging.
#include "RAS_BucketManager.h" // XXX cant stay
#include "KX_BlenderSceneConverter.h"
#include "KX_MeshProxy.h"

extern "C" {
	#  include "PIL_time.h"
	#  include "BKE_context.h"
	#  include "BLO_readfile.h"
	#  include "BKE_idcode.h"
	#  include "BKE_report.h"
	#  include "DNA_space_types.h"
	#  include "DNA_windowmanager_types.h" // report api
	#  include "../../blender/blenlib/BLI_linklist.h"
}

#include "BLI_task.h"
#include "CM_Thread.h"
#include "CM_Message.h"

<<<<<<< HEAD
typedef struct ThreadInfo {
	TaskPool *m_pool;
	CM_ThreadMutex m_mutex;
} ThreadInfo;

KX_BlenderSceneConverter::KX_BlenderSceneConverter(Main *maggie, KX_KetsjiEngine *engine)
	:m_maggie(maggie),
	m_ketsjiEngine(engine),
	m_alwaysUseExpandFraming(false)
{
	BKE_main_id_tag_all(maggie, LIB_TAG_DOIT, false);  // avoid re-tagging later on
	m_newfilename = "";
	m_threadinfo = new ThreadInfo();
	m_threadinfo->m_pool = BLI_task_pool_create(engine->GetTaskScheduler(), nullptr);
}

KX_BlenderSceneConverter::~KX_BlenderSceneConverter()
{
	// clears meshes, and hashmaps from blender to gameengine data
	// delete sumoshapes

	for (std::map<bAction *, BL_InterpolatorList *>::iterator it = m_map_blender_to_gameAdtList.begin(),
	     end = m_map_blender_to_gameAdtList.end(); it != end; ++it)
	{
		delete it->second;
	}

	for (std::map<KX_Scene *, std::vector<RAS_IPolyMaterial *> >::iterator polymit = m_polymaterials.begin(),
	     polymend = m_polymaterials.end(); polymit != polymend; ++polymit)
	{
		std::vector<RAS_IPolyMaterial *> &polymatlist = polymit->second;
		for (std::vector<RAS_IPolyMaterial *>::iterator it = polymatlist.begin(), end = polymatlist.end(); it != end; ++it) {
			delete *it;
		}
	}

	for (std::map<KX_Scene *, std::vector<RAS_MeshObject *> >::iterator meshit = m_meshobjects.begin(), meshend = m_meshobjects.end();
	     meshit != meshend; ++meshit)
	{
		std::vector<RAS_MeshObject *> &meshlist = meshit->second;
		for (std::vector<RAS_MeshObject *>::iterator it = meshlist.begin(), end = meshlist.end(); it != end; ++it) {
			delete *it;
		}
	}

	// free any data that was dynamically loaded
	while (m_DynamicMaggie.size() != 0) {
		FreeBlendFile(m_DynamicMaggie[0]);
	}

	m_DynamicMaggie.clear();

	if (m_threadinfo) {
		/* Thread infos like mutex must be freed after FreeBlendFile function.
		   Because it needs to lock the mutex, even if there's no active task when it's
		   in the scene converter destructor. */
		BLI_task_pool_free(m_threadinfo->m_pool);
		delete m_threadinfo;
	}
}

void KX_BlenderSceneConverter::SetNewFileName(const std::string &filename)
{
	m_newfilename = filename;
}

bool KX_BlenderSceneConverter::TryAndLoadNewFile()
{
	bool result = false;

	return result;
}

Scene *KX_BlenderSceneConverter::GetBlenderSceneForName(const std::string &name)
{
	Scene *sce;

	// Find the specified scene by name, or nullptr if nothing matches.
	if ((sce = (Scene *)BLI_findstring(&m_maggie->scene, name.c_str(), offsetof(ID, name) + 2))) {
		return sce;
	}

	for (std::vector<Main *>::iterator it = m_DynamicMaggie.begin(); !(it == m_DynamicMaggie.end()); it++) {
		Main *main = *it;

		if ((sce = (Scene *)BLI_findstring(&main->scene, name.c_str(), offsetof(ID, name) + 2))) {
			return sce;
		}
	}

	return nullptr;
}

CListValue *KX_BlenderSceneConverter::GetInactiveSceneNames()
{
	CListValue *list = new CListValue();

	for (Scene *sce = (Scene *)m_maggie->scene.first; sce; sce = (Scene *)sce->id.next) {
		const char *name = sce->id.name + 2;
		if (m_ketsjiEngine->CurrentScenes()->FindValue(name)) {
			continue;
		}
		CStringValue *item = new CStringValue(name, name);
		list->Add(item);
	}

	return list;
}

void KX_BlenderSceneConverter::ConvertScene(KX_Scene *destinationscene, RAS_Rasterizer *rendertools,
                                            RAS_ICanvas *canvas, bool libloading)
{
	//find out which physics engine
	Scene *blenderscene = destinationscene->GetBlenderScene();

	PHY_IPhysicsEnvironment *phy_env = nullptr;

	e_PhysicsEngine physics_engine = UseBullet;
	destinationscene->SetSceneConverter(this);

	// This doesn't really seem to do anything except cause potential issues
	// when doing threaded conversion, so it's disabled for now.
	// SG_SetActiveStage(SG_STAGE_CONVERTER);

	switch (blenderscene->gm.physicsEngine)
	{
#ifdef WITH_BULLET
		case WOPHY_BULLET:
		{
			SYS_SystemHandle syshandle = SYS_GetSystem(); /*unused*/
			int visualizePhysics = SYS_GetCommandLineInt(syshandle, "show_physics", 0);

			phy_env = CcdPhysicsEnvironment::Create(blenderscene, visualizePhysics);
			physics_engine = UseBullet;
			break;
		}
#endif
		default:
		case WOPHY_NONE:
		{
			// We should probably use some sort of factory here
			phy_env = new DummyPhysicsEnvironment();
			physics_engine = UseNone;
			break;
		}
	}

	destinationscene->SetPhysicsEnvironment(phy_env);

	BL_ConvertBlenderObjects(
		m_maggie,
		destinationscene,
		m_ketsjiEngine,
		physics_engine,
		rendertools,
		canvas,
		this,
		m_alwaysUseExpandFraming,
		libloading);

	//These lookup are not needed during game
	m_map_blender_to_gameactuator.clear();
	m_map_blender_to_gamecontroller.clear();
	m_map_blender_to_gameobject.clear();

	//Clearing this lookup table has the effect of disabling the cache of meshes
	//between scenes, even if they are shared in the blend file.
	//This cache mecanism is buggy so I leave it disable and the memory leak
	//that would result from this is fixed in RemoveScene()
	m_map_mesh_to_gamemesh.clear();
}

/** This function removes all entities stored in the converter for that scene
 * It should be used instead of direct delete scene
 * Note that there was some provision for sharing entities (meshes...) between
 * scenes but that is now disabled so all scene will have their own copy
 * and we can delete them here. If the sharing is reactivated, change this code too..
 * (see KX_BlenderSceneConverter::ConvertScene)
 */
void KX_BlenderSceneConverter::RemoveScene(KX_Scene *scene)
{
	KX_WorldInfo *world = scene->GetWorldInfo();
	if (world) {
		delete world;
	}

#ifdef WITH_PYTHON
	Texture::FreeAllTextures(scene);
#endif  // WITH_PYTHON

	// delete the scene first as it will stop the use of entities
	scene->Release();

	// delete the entities of this scene

	std::map<KX_Scene *, std::vector<RAS_IPolyMaterial *> >::iterator polymit = m_polymaterials.find(scene);
	if (polymit != m_polymaterials.end()) {
		std::vector<RAS_IPolyMaterial *> &polymatlist = polymit->second;
		for (std::vector<RAS_IPolyMaterial *>::iterator it = polymatlist.begin(), end = polymatlist.end(); it != end; ++it) {
			delete *it;
		}
		m_polymaterials.erase(polymit);
	}

	std::map<KX_Scene *, std::vector<RAS_MeshObject *> >::iterator meshit = m_meshobjects.find(scene);
	if (meshit != m_meshobjects.end()) {
		std::vector<RAS_MeshObject *> &meshlist = meshit->second;
		for (std::vector<RAS_MeshObject *>::iterator it = meshlist.begin(), end = meshlist.end(); it != end; ++it) {
			delete *it;
		}
		m_meshobjects.erase(meshit);
	}

	m_polymat_cache.erase(scene);
}

void KX_BlenderSceneConverter::SetAlwaysUseExpandFraming(bool to_what)
{
	m_alwaysUseExpandFraming = to_what;
}

=======
>>>>>>> temp
void KX_BlenderSceneConverter::RegisterGameObject(KX_GameObject *gameobject, Object *for_blenderobject)
{
	CM_FunctionDebug("object name: " << gameobject->GetName());
	// only maintained while converting, freed during game runtime
	m_map_blender_to_gameobject[for_blenderobject] = gameobject;
}

/** only need to run this during conversion since
 * m_map_blender_to_gameobject is freed after conversion */
void KX_BlenderSceneConverter::UnregisterGameObject(KX_GameObject *gameobject)
{
	Object *bobp = gameobject->GetBlenderObject();
	if (bobp) {
		std::map<Object *, KX_GameObject *>::iterator it = m_map_blender_to_gameobject.find(bobp);
		if (it->second == gameobject) {
			// also maintain m_map_blender_to_gameobject if the gameobject
			// being removed is matching the blender object
			m_map_blender_to_gameobject.erase(it);
		}
	}
}

KX_GameObject *KX_BlenderSceneConverter::FindGameObject(Object *for_blenderobject)
{
	return m_map_blender_to_gameobject[for_blenderobject];
}

void KX_BlenderSceneConverter::RegisterGameMesh(KX_Scene *scene, RAS_MeshObject *gamemesh, Mesh *for_blendermesh)
{
	if (for_blendermesh) { // dynamically loaded meshes we don't want to keep lookups for
		m_map_mesh_to_gamemesh[for_blendermesh] = gamemesh;
	}
	m_meshobjects[scene].push_back(gamemesh);
}

RAS_MeshObject *KX_BlenderSceneConverter::FindGameMesh(Mesh *for_blendermesh)
{
	return m_map_mesh_to_gamemesh[for_blendermesh];
}

void KX_BlenderSceneConverter::RegisterPolyMaterial(KX_Scene *scene, Material *mat, RAS_IPolyMaterial *polymat)
{
	if (mat) {
		m_polymat_cache[scene][mat].push_back(polymat);
	}
	m_polymaterials[scene].push_back(polymat);
}

void KX_BlenderSceneConverter::CachePolyMaterial(KX_Scene *scene, Material *mat, RAS_IPolyMaterial *polymat)
{
	if (mat) {
		m_polymat_cache[scene][mat] = polymat;
	}
}

RAS_IPolyMaterial *KX_BlenderSceneConverter::FindCachedPolyMaterial(KX_Scene *scene, Material *mat)
{
	return m_polymat_cache[scene][mat];
}

void KX_BlenderSceneConverter::RegisterInterpolatorList(BL_InterpolatorList *actList, bAction *for_act)
{
	m_map_blender_to_gameAdtList[for_act] = actList;
}

BL_InterpolatorList *KX_BlenderSceneConverter::FindInterpolatorList(bAction *for_act)
{
	return m_map_blender_to_gameAdtList[for_act];
}

void KX_BlenderSceneConverter::RegisterGameActuator(SCA_IActuator *act, bActuator *for_actuator)
{
	m_map_blender_to_gameactuator[for_actuator] = act;
}

SCA_IActuator *KX_BlenderSceneConverter::FindGameActuator(bActuator *for_actuator)
{
	return m_map_blender_to_gameactuator[for_actuator];
}

void KX_BlenderSceneConverter::RegisterGameController(SCA_IController *cont, bController *for_controller)
{
	m_map_blender_to_gamecontroller[for_controller] = cont;
}

SCA_IController *KX_BlenderSceneConverter::FindGameController(bController *for_controller)
{
	return m_map_blender_to_gamecontroller[for_controller];
}
