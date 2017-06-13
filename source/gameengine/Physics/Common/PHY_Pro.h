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

/** \file PHY_Pro.h
 *  \ingroup phys
 */

#ifndef __PHY_PRO_H__
#define __PHY_PRO_H__

// Properties of dynamic objects
struct PHY_ShapeProps {
	float m_mass; // Total mass
	float m_inertia; // Inertia, should be a tensor some time
	float m_lin_drag; // Linear drag (air, water) 0 = concrete, 1 = vacuum, inverted and called dampening in blenders UI
	float m_ang_drag; // Angular drag, inverted and called dampening in blenders UI
	MT_Vector3 m_friction_scaling; // Scaling for anisotropic friction. Component in range [0, 1]
	float m_clamp_vel_min; // Clamp the minimum velocity, this ensures an object moves at a minimum speed unless its stationary
	float m_clamp_vel_max; // Clamp max velocity
	float m_clamp_angvel_min; // Clamp the minimum angular velocity.
	float m_clamp_angvel_max; // Clamp the maximum angular velocity.
	bool m_do_anisotropic; // Should I do anisotropic friction?
	bool m_do_fh; // Should the object have a linear Fh spring?
	bool m_do_rot_fh; // Should the object have an angular Fh spring?
	float m_restitution; // restitution of energie after a collision 0 = inelastic, 1 = elastic
	float m_friction; // Coulomb friction (= ratio between the normal en maximum friction force)
	float m_rollingFriction; // Friction used for rounded shapes.
	float m_fh_spring; // Spring constant (both linear and angular)
	float m_fh_damping; // Damping factor (linear and angular) in range [0, 1]
	float m_fh_distance; // The range above the surface where Fh is active.
	bool m_fh_normal; // Should the object slide off slopes?
	float m_step_height; // Max height of climbable steps (Character)
	float m_jump_speed; // Velocity of jumps (Character)
	float m_fall_speed; // Max velocity of falling (Character)
	unsigned char m_max_jumps; // Max ammount of jumps (Character)
};

#endif  /* __PHY_PRO_H__ */
