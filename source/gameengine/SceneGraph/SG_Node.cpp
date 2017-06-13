/* ***** BEGIN GPL LICENSE BLOCK *****
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

/** \file gameengine/SceneGraph/SG_Node.cpp
 *  \ingroup bgesg
 */


#include "SG_Node.h"
#include "SG_Familly.h"
#include "SG_Controller.h"

#include <algorithm>

static CM_ThreadMutex scheduleMutex;
static CM_ThreadMutex transformMutex;

SG_Node::SG_Node(void *clientobj, void *clientinfo, SG_Callbacks& callbacks)
	:SG_QList(),
	m_SGclientObject(clientobj),
	m_SGclientInfo(clientinfo),
	m_callbacks(callbacks),
	m_SGparent(nullptr),
	m_localPosition(0.0f, 0.0f, 0.0f),
	m_localRotation(1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f),
	m_localScaling(1.0f, 1.0f, 1.0f),
	m_worldPosition(0.0f, 0.0f, 0.0f),
	m_worldRotation(1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f),
	m_worldScaling(1.0f, 1.0f, 1.0f),
	m_parent_relation(nullptr),
	m_familly(new SG_Familly()),
	m_modified(true),
	m_ogldirty(false)
{
}

SG_Node::SG_Node(const SG_Node & other)
	:SG_QList(),
	m_SGclientObject(other.m_SGclientObject),
	m_SGclientInfo(other.m_SGclientInfo),
	m_callbacks(other.m_callbacks),
	m_children(other.m_children),
	m_SGparent(other.m_SGparent),
	m_localPosition(other.m_localPosition),
	m_localRotation(other.m_localRotation),
	m_localScaling(other.m_localScaling),
	m_worldPosition(other.m_worldPosition),
	m_worldRotation(other.m_worldRotation),
	m_worldScaling(other.m_worldScaling),
	m_parent_relation(other.m_parent_relation->NewCopy()),
	m_familly(new SG_Familly()),
	m_ogldirty(false)
{
}

SG_Node::~SG_Node()
{
	SGControllerList::iterator contit;

	for (contit = m_SGcontrollers.begin(); contit != m_SGcontrollers.end(); ++contit) {
		delete (*contit);
	}
}

SG_Node *SG_Node::GetSGReplica()
{
	SG_Node *replica = new SG_Node(*this);
	if (replica == nullptr) {
		return nullptr;
	}

	ProcessSGReplica(&replica);

	return replica;
}

void SG_Node::ProcessSGReplica(SG_Node **replica)
{
	// Apply the replication call back function.
	if (!ActivateReplicationCallback(*replica)) {
		delete (*replica);
		*replica = nullptr;
		return;
	}

	// clear the replica node of it's parent.
	(*replica)->m_SGparent = nullptr;

	if (m_children.size() > 0) {
		// if this node has children, the replica has too, so clear and clone children
		(*replica)->ClearSGChildren();

		for (SG_Node *childnode : m_children) {
			SG_Node *replicanode = childnode->GetSGReplica();
			if (replicanode) {
				(*replica)->AddChild(replicanode);
			}
		}
	}
	// Nodes without children and without client object are
	// not worth to keep, they will just take up CPU
	// This can happen in partial replication of hierarchy
	// during group duplication.
	if ((*replica)->m_children.size() == 0 &&
	    (*replica)->GetSGClientObject() == nullptr)
	{
		delete (*replica);
		*replica = nullptr;
	}
}

void SG_Node::Destruct()
{
	// Not entirely sure what Destruct() expects to happen.
	// I think it probably means just to call the DestructionCallback
	// in the right order on all the children - rather than free any memory

	// We'll delete m_parent_relation now anyway.

	m_parent_relation.reset(nullptr);

	for (SG_Node *childnode : m_children) {
		// call the SG_Node destruct method on each of our children }-)
		childnode->Destruct();
	}

	ActivateDestructionCallback();
}

const SG_Node *SG_Node::GetRootSGParent() const
{
	return (m_SGparent ? m_SGparent->GetRootSGParent() : this);
}

bool SG_Node::IsAncessor(const SG_Node *child) const
{
	return (!child->m_SGparent) ? false :
	       (child->m_SGparent == this) ? true : IsAncessor(child->m_SGparent);
}

NodeList& SG_Node::GetSGChildren()
{
	return m_children;
}

void SG_Node::ClearSGChildren()
{
	m_children.clear();
}

SG_Node *SG_Node::GetSGParent() const
{
	return m_SGparent;
}

void SG_Node::SetSGParent(SG_Node *parent)
{
	m_SGparent = parent;
	if (parent) {
		SetFamilly(parent->GetFamilly());
	}
}

void SG_Node::DisconnectFromParent()
{
	if (m_SGparent) {
		m_SGparent->RemoveChild(this);
		m_SGparent = nullptr;
		SetFamilly(std::make_shared<SG_Familly>());
	}
}

bool SG_Node::IsVertexParent()
{
	if (m_parent_relation) {
		return m_parent_relation->IsVertexRelation();
	}
	return false;
}

bool SG_Node::IsSlowParent()
{
	if (m_parent_relation) {
		return m_parent_relation->IsSlowRelation();
	}
	return false;
}

void SG_Node::AddChild(SG_Node *child)
{
	m_children.push_back(child);
	child->SetSGParent(this);
}

void SG_Node::RemoveChild(SG_Node *child)
{
	m_children.erase(std::find(m_children.begin(), m_children.end(), child));
}

void SG_Node::UpdateWorldData(double time, bool parentUpdated)
{
	if (UpdateSpatialData(GetSGParent(), time, parentUpdated)) {
		// to update the
		ActivateUpdateTransformCallback();
	}

	// The node is updated, remove it from the update list
	Delink();

	// update children's worlddata
	for (SG_Node *childnode : m_children) {
		childnode->UpdateWorldData(time, parentUpdated);
	}
}

void SG_Node::UpdateWorldDataThread(double time, bool parentUpdated)
{
	CM_ThreadSpinLock& famillyMutex = m_familly->GetMutex();
	famillyMutex.Lock();

	UpdateWorldDataThreadSchedule(time, parentUpdated);

	famillyMutex.Unlock();
}

void SG_Node::UpdateWorldDataThreadSchedule(double time, bool parentUpdated)
{
	if (UpdateSpatialData(GetSGParent(), time, parentUpdated)) {
		// to update the
		ActivateUpdateTransformCallback();
	}

	scheduleMutex.Lock();
	// The node is updated, remove it from the update list
	Delink();
	scheduleMutex.Unlock();

	// update children's worlddata
	for (SG_Node *childnode : m_children) {
		childnode->UpdateWorldDataThreadSchedule(time, parentUpdated);
	}
}

void SG_Node::SetSimulatedTime(double time, bool recurse)
{
	// update the controllers of this node.
	SetControllerTime(time);

	// update children's simulate time.
	if (recurse) {
		for (SG_Node *childnode : m_children) {
			childnode->SetSimulatedTime(time, recurse);
		}
	}
}

void SG_Node::SetSimulatedTimeThread(double time, bool recurse)
{
	CM_ThreadSpinLock& famillyMutex = m_familly->GetMutex();
	famillyMutex.Lock();
	// update the controllers of this node.
	SetControllerTime(time);

	// update children's simulate time.
	if (recurse) {
		for (SG_Node *childnode : m_children) {
			childnode->SetSimulatedTime(time, recurse);
		}
	}
	famillyMutex.Unlock();
}

bool SG_Node::Schedule(SG_QList& head)
{
	scheduleMutex.Lock();
	// Put top parent in front of list to make sure they are updated before their
	// children => the children will be udpated and removed from the list before
	// we get to them, should they be in the list too.
	const bool result = (m_SGparent) ? head.AddBack(this) : head.AddFront(this);
	scheduleMutex.Unlock();

	return result;
}

SG_Node *SG_Node::GetNextScheduled(SG_QList& head)
{
	scheduleMutex.Lock();
	SG_Node *result = static_cast<SG_Node *>(head.Remove());
	scheduleMutex.Unlock();

	return result;
}

bool SG_Node::Reschedule(SG_QList& head)
{
	scheduleMutex.Lock();
	const bool result = head.QAddBack(this);
	scheduleMutex.Unlock();

	return result;
}

SG_Node *SG_Node::GetNextRescheduled(SG_QList& head)
{
	scheduleMutex.Lock();
	SG_Node *result = static_cast<SG_Node *>(head.QRemove());
	scheduleMutex.Unlock();

	return result;
}

void SG_Node::AddSGController(SG_Controller *cont)
{
	m_SGcontrollers.push_back(cont);
}

void SG_Node::RemoveSGController(SG_Controller *cont)
{
	m_mutex.Lock();
	m_SGcontrollers.erase(std::find(m_SGcontrollers.begin(), m_SGcontrollers.end(), cont));
	m_mutex.Unlock();
}

void SG_Node::RemoveAllControllers()
{
	m_SGcontrollers.clear();
}

SGControllerList& SG_Node::GetSGControllerList()
{
	return m_SGcontrollers;
}

SG_Callbacks& SG_Node::GetCallBackFunctions()
{
	return m_callbacks;
}

void *SG_Node::GetSGClientObject() const
{
	return m_SGclientObject;
}

void SG_Node::SetSGClientObject(void *clientObject)
{
	m_SGclientObject = clientObject;
}

void *SG_Node::GetSGClientInfo() const
{
	return m_SGclientInfo;
}
void SG_Node::SetSGClientInfo(void *clientInfo)
{
	m_SGclientInfo = clientInfo;
}

void SG_Node::SetControllerTime(double time)
{
	for (SG_Controller *cont : m_SGcontrollers) {
		cont->SetSimulatedTime(time);
	}
}

void SG_Node::ClearModified()
{
	m_modified = false;
	m_ogldirty = true;
}

void SG_Node::SetModified()
{
	m_modified = true;
	ActivateScheduleUpdateCallback();
}

void SG_Node::ClearDirty()
{
	m_ogldirty = false;
}

void SG_Node::SetParentRelation(SG_ParentRelation *relation)
{
	m_parent_relation.reset(relation);
	SetModified();
}

SG_ParentRelation *SG_Node::GetParentRelation()
{
	return m_parent_relation.get();
}

/**
 * Update Spatial Data.
 * Calculates WorldTransform., (either doing its self or using the linked SGControllers)
 */
bool SG_Node::UpdateSpatialData(const SG_Node *parent, double time, bool& parentUpdated)
{
	bool bComputesWorldTransform = false;

	// update spatial controllers
	for (SG_Controller *cont : m_SGcontrollers) {
		if (cont->Update(time)) {
			bComputesWorldTransform = true;
		}
	}

	// If none of the objects updated our values then we ask the
	// parent_relation object owned by this class to update
	// our world coordinates.

	if (!bComputesWorldTransform) {
		bComputesWorldTransform = ComputeWorldTransforms(parent, parentUpdated);
	}

	return bComputesWorldTransform;
}

/**
 * Position and translation methods
 */
void SG_Node::RelativeTranslate(const MT_Vector3& trans, const SG_Node *parent, bool local)
{
	if (local) {
		m_localPosition += m_localRotation * trans;
	}
	else {
		if (parent) {
			m_localPosition += trans * parent->GetWorldOrientation();
		}
		else {
			m_localPosition += trans;
		}
	}
	SetModified();
}

void SG_Node::SetLocalPosition(const MT_Vector3& trans)
{
	m_localPosition = trans;
	SetModified();
}

void SG_Node::SetWorldPosition(const MT_Vector3& trans)
{
	m_worldPosition = trans;
}

/**
 * Scaling methods.
 */

/**
 * Orientation and rotation methods.
 */
void SG_Node::RelativeRotate(const MT_Matrix3x3& rot, bool local)
{
	m_localRotation = m_localRotation * (
		local ?
		rot
		:
		(GetWorldOrientation().inverse() * rot * GetWorldOrientation()));
	SetModified();
}

void SG_Node::SetLocalOrientation(const MT_Matrix3x3& rot)
{
	m_localRotation = rot;
	SetModified();
}

void SG_Node::SetWorldOrientation(const MT_Matrix3x3& rot)
{
	m_worldRotation = rot;
}

void SG_Node::RelativeScale(const MT_Vector3& scale)
{
	m_localScaling = m_localScaling * scale;
	SetModified();
}

void SG_Node::SetLocalScale(const MT_Vector3& scale)
{
	m_localScaling = scale;
	SetModified();
}

void SG_Node::SetWorldScale(const MT_Vector3& scale)
{
	m_worldScaling = scale;
}

const MT_Vector3& SG_Node::GetLocalPosition() const
{
	return m_localPosition;
}

const MT_Matrix3x3& SG_Node::GetLocalOrientation() const
{
	return m_localRotation;
}

const MT_Vector3& SG_Node::GetLocalScale() const
{
	return m_localScaling;
}

const MT_Vector3& SG_Node::GetWorldPosition() const
{
	return m_worldPosition;
}

const MT_Matrix3x3& SG_Node::GetWorldOrientation() const
{
	return m_worldRotation;
}

const MT_Vector3& SG_Node::GetWorldScaling() const
{
	return m_worldScaling;
}

void SG_Node::SetWorldFromLocalTransform()
{
	m_worldPosition = m_localPosition;
	m_worldScaling = m_localScaling;
	m_worldRotation = m_localRotation;
}

MT_Transform SG_Node::GetWorldTransform() const
{
	return MT_Transform(m_worldPosition,
	                    m_worldRotation.scaled(
							m_worldScaling[0], m_worldScaling[1], m_worldScaling[2]));
}

bool SG_Node::ComputeWorldTransforms(const SG_Node *parent, bool& parentUpdated)
{
	return m_parent_relation->UpdateChildCoordinates(this, parent, parentUpdated);
}

const std::shared_ptr<SG_Familly>& SG_Node::GetFamilly() const
{
	BLI_assert(m_familly != nullptr);

	return m_familly;
}

void SG_Node::SetFamilly(const std::shared_ptr<SG_Familly>& familly)
{
	BLI_assert(familly != nullptr);

	m_familly = familly;
	for (SG_Node *child : m_children) {
		child->SetFamilly(m_familly);
	}
}

bool SG_Node::IsModified()
{
	return m_modified;
}
bool SG_Node::IsDirty()
{
	return m_ogldirty;
}

bool SG_Node::ActivateReplicationCallback(SG_Node *replica)
{
	if (m_callbacks.m_replicafunc) {
		// Call client provided replication func
		if (m_callbacks.m_replicafunc(replica, m_SGclientObject, m_SGclientInfo) == nullptr) {
			return false;
		}
	}
	return true;
}

void SG_Node::ActivateDestructionCallback()
{
	if (m_callbacks.m_destructionfunc) {
		// Call client provided destruction function on this!
		m_callbacks.m_destructionfunc(this, m_SGclientObject, m_SGclientInfo);
	}
	else {
		// no callback but must still destroy the node to avoid memory leak
		delete this;
	}
}

void SG_Node::ActivateUpdateTransformCallback()
{
	if (m_callbacks.m_updatefunc) {
		// Call client provided update func.
		transformMutex.Lock();
		m_callbacks.m_updatefunc(this, m_SGclientObject, m_SGclientInfo);
		transformMutex.Unlock();
	}
}

bool SG_Node::ActivateScheduleUpdateCallback()
{
	// HACK, this check assumes that the scheduled nodes are put on a DList (see SG_Node.h)
	// The early check on Empty() allows up to avoid calling the callback function
	// when the node is already scheduled for update.
	scheduleMutex.Lock();
	const bool empty = Empty();
	scheduleMutex.Unlock();

	if (empty && m_callbacks.m_schedulefunc) {
		// Call client provided update func.
		return m_callbacks.m_schedulefunc(this, m_SGclientObject, m_SGclientInfo);
	}
	return false;
}

void SG_Node::ActivateRecheduleUpdateCallback()
{
	if (m_callbacks.m_reschedulefunc) {
		// Call client provided update func.
		m_callbacks.m_reschedulefunc(this, m_SGclientObject, m_SGclientInfo);
	}
}
