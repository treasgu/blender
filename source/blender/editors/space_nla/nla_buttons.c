/**
 * $Id:
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation, Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_editVert.h"
#include "BLI_rand.h"

#include "BKE_animsys.h"
#include "BKE_nla.h"
#include "BKE_action.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_fcurve.h"
#include "BKE_object.h"
#include "BKE_global.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"

#include "BIF_gl.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "ED_anim_api.h"
#include "ED_keyframing.h"
#include "ED_screen.h"
#include "ED_types.h"
#include "ED_util.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "nla_intern.h"	// own include


/* ******************* nla editor space & buttons ************** */

#define B_NOP		1
#define B_REDR		2

/* -------------- */

static void do_nla_region_buttons(bContext *C, void *arg, int event)
{
	//Scene *scene= CTX_data_scene(C);
	
	switch(event) {

	}
	
	/* default for now */
	WM_event_add_notifier(C, NC_SCENE|NC_OBJECT|ND_TRANSFORM, NULL);
}

static int nla_panel_context(const bContext *C, PointerRNA *nlt_ptr, PointerRNA *strip_ptr)
{
	bAnimContext ac;
	bAnimListElem *ale= NULL;
	ListBase anim_data = {NULL, NULL};
	short found=0;
	int filter;
	
	/* for now, only draw if we could init the anim-context info (necessary for all animation-related tools) 
	 * to work correctly is able to be correctly retrieved. There's no point showing empty panels?
	 */
	if (ANIM_animdata_get_context(C, &ac) == 0) 
		return 0;
	
	/* extract list of active channel(s), of which we should only take the first one (expecting it to be an NLA track) */
	filter= (ANIMFILTER_VISIBLE|ANIMFILTER_ACTIVE);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	for (ale= anim_data.first; ale; ale= ale->next) {
		if (ale->type == ANIMTYPE_NLATRACK) {
			NlaTrack *nlt= (NlaTrack *)ale->data;
			
			/* found it, now set the pointers */
			if (nlt_ptr) {
				/* NLA-Track pointer */
				RNA_pointer_create(ale->id, &RNA_NlaTrack, nlt, nlt_ptr);
			}
			if (strip_ptr) {
				/* NLA-Strip pointer */
				NlaStrip *strip= BKE_nlastrip_find_active(nlt);
				RNA_pointer_create(ale->id, &RNA_NlaStrip, strip, strip_ptr);
			}
			
			found= 1;
			break;
		}
	}
	
	/* free temp data */
	BLI_freelistN(&anim_data);
	
	return found;
}

#if 0
static int nla_panel_poll(const bContext *C, PanelType *pt)
{
	return nla_panel_context(C, NULL, NULL);
}
#endif

static int nla_track_panel_poll(const bContext *C, PanelType *pt)
{
	PointerRNA ptr;
	return (nla_panel_context(C, &ptr, NULL) && (ptr.data != NULL));
}

static int nla_strip_panel_poll(const bContext *C, PanelType *pt)
{
	PointerRNA ptr;
	return (nla_panel_context(C, NULL, &ptr) && (ptr.data != NULL));
}

static int nla_strip_actclip_panel_poll(const bContext *C, PanelType *pt)
{
	PointerRNA ptr;
	NlaStrip *strip;
	
	if (!nla_panel_context(C, NULL, &ptr))
		return 0;
	if (ptr.data == NULL)
		return 0;
	
	strip= ptr.data;
	return (strip->type == NLASTRIP_TYPE_CLIP);
}

/* -------------- */

/* active NLA-Track */
static void nla_panel_track (const bContext *C, Panel *pa)
{
	PointerRNA nlt_ptr;
	uiLayout *layout= pa->layout;
	uiLayout *row;
	uiBlock *block;
	
	/* check context and also validity of pointer */
	if (!nla_panel_context(C, &nlt_ptr, NULL))
		return;

	block= uiLayoutGetBlock(layout);
	uiBlockSetHandleFunc(block, do_nla_region_buttons, NULL);
	
	/* Info - Active NLA-Context:Track ----------------------  */
	row= uiLayoutRow(layout, 1);
		uiItemR(row, NULL, ICON_NLA, &nlt_ptr, "name", 0, 0, 0);
}

/* generic settings for active NLA-Strip */
static void nla_panel_properties(const bContext *C, Panel *pa)
{
	PointerRNA strip_ptr;
	uiLayout *layout= pa->layout;
	uiLayout *column, *row, *subcol;
	uiBlock *block;
	
	if (!nla_panel_context(C, NULL, &strip_ptr))
		return;
	
	block= uiLayoutGetBlock(layout);
	uiBlockSetHandleFunc(block, do_nla_region_buttons, NULL);
	
	/* Strip Properties ------------------------------------- */
	/* strip type */
	row= uiLayoutRow(layout, 1);
		uiItemR(row, NULL, 0, &strip_ptr, "type", 0, 0, 0);
	
	/* strip extents */
	column= uiLayoutColumn(layout, 1);
		uiItemL(column, "Strip Extents:", 0);
		uiItemR(column, NULL, 0, &strip_ptr, "start_frame", 0, 0, 0);
		uiItemR(column, NULL, 0, &strip_ptr, "end_frame", 0, 0, 0);
	
	/* extrapolation */
	row= uiLayoutRow(layout, 1);
		uiItemR(row, NULL, 0, &strip_ptr, "extrapolation", 0, 0, 0);
	
	/* blending */
	row= uiLayoutRow(layout, 1);
		uiItemR(row, NULL, 0, &strip_ptr, "blending", 0, 0, 0);
		
	/* blend in/out + autoblending
	 *	- blend in/out can only be set when autoblending is off
	 */
	column= uiLayoutColumn(layout, 1);
		uiItemR(column, NULL, 0, &strip_ptr, "auto_blending", 0, 0, 0); // XXX as toggle?
	subcol= uiLayoutColumn(column, 1);
		uiLayoutSetActive(subcol, RNA_boolean_get(&strip_ptr, "auto_blending")==0); 
		uiItemR(subcol, NULL, 0, &strip_ptr, "blend_in", 0, 0, 0);
		uiItemR(subcol, NULL, 0, &strip_ptr, "blend_out", 0, 0, 0);
		
	/* settings */
	column= uiLayoutColumn(layout, 1);
		uiItemL(column, "Playback Settings:", 0);
		uiItemR(column, NULL, 0, &strip_ptr, "muted", 0, 0, 0);
		uiItemR(column, NULL, 0, &strip_ptr, "reversed", 0, 0, 0);
}


/* action-clip only settings for active NLA-Strip */
static void nla_panel_actclip(const bContext *C, Panel *pa)
{
	PointerRNA strip_ptr;
	uiLayout *layout= pa->layout;
	uiLayout *column, *row;
	uiBlock *block;

	/* check context and also validity of pointer */
	if (!nla_panel_context(C, NULL, &strip_ptr))
		return;
	
	block= uiLayoutGetBlock(layout);
	uiBlockSetHandleFunc(block, do_nla_region_buttons, NULL);
		
	/* Strip Properties ------------------------------------- */
	/* action pointer */
	row= uiLayoutRow(layout, 1);
		uiItemR(row, NULL, ICON_ACTION, &strip_ptr, "action", 0, 0, 0);
		
	/* action extents */
	// XXX custom names were used here... probably not necessary in future?
	column= uiLayoutColumn(layout, 1);
		uiItemL(column, "Action Extents:", 0);
		uiItemR(column, "Start Frame", 0, &strip_ptr, "action_start_frame", 0, 0, 0);
		uiItemR(column, "End Frame", 0, &strip_ptr, "action_end_frame", 0, 0, 0);
		
	/* action usage */
	column= uiLayoutColumn(layout, 1);
		uiItemL(column, "Playback Settings:", 0);
		uiItemR(column, NULL, 0, &strip_ptr, "scale", 0, 0, 0);
		uiItemR(column, NULL, 0, &strip_ptr, "repeat", 0, 0, 0);
}

/* evaluation settings for active NLA-Strip */
static void nla_panel_evaluation(const bContext *C, Panel *pa)
{
	PointerRNA strip_ptr;
	uiLayout *layout= pa->layout;
	//uiLayout *column, *row, *subcol;
	uiBlock *block;

	/* check context and also validity of pointer */
	if (!nla_panel_context(C, NULL, &strip_ptr))
		return;
		
	block= uiLayoutGetBlock(layout);
	uiBlockSetHandleFunc(block, do_nla_region_buttons, NULL);
		
	// influence
	// strip_time
	// animated_influence
	// animated_time
}

/* F-Modifiers for active NLA-Strip */
static void nla_panel_modifiers(const bContext *C, Panel *pa)
{
	PointerRNA strip_ptr;
	uiLayout *layout= pa->layout;
	//uiLayout *column, *row, *subcol;
	uiBlock *block;

	/* check context and also validity of pointer */
	if (!nla_panel_context(C, NULL, &strip_ptr))
		return;
		
	block= uiLayoutGetBlock(layout);
	uiBlockSetHandleFunc(block, do_nla_region_buttons, NULL);
		
	// TODO...
}

/* ******************* general ******************************** */


void nla_buttons_register(ARegionType *art)
{
	PanelType *pt;
	
	pt= MEM_callocN(sizeof(PanelType), "spacetype nla panel track");
	strcpy(pt->idname, "NLA_PT_track");
	strcpy(pt->label, "Active Track");
	pt->draw= nla_panel_track;
	pt->poll= nla_track_panel_poll;
	BLI_addtail(&art->paneltypes, pt);
	
	pt= MEM_callocN(sizeof(PanelType), "spacetype nla panel properties");
	strcpy(pt->idname, "NLA_PT_properties");
	strcpy(pt->label, "Active Strip");
	pt->draw= nla_panel_properties;
	pt->poll= nla_strip_panel_poll;
	BLI_addtail(&art->paneltypes, pt);
	
	pt= MEM_callocN(sizeof(PanelType), "spacetype nla panel properties");
	strcpy(pt->idname, "NLA_PT_actionclip");
	strcpy(pt->label, "Action Clip");
	pt->draw= nla_panel_actclip;
	pt->poll= nla_strip_actclip_panel_poll;
	BLI_addtail(&art->paneltypes, pt);
	
	pt= MEM_callocN(sizeof(PanelType), "spacetype nla panel evaluation");
	strcpy(pt->idname, "NLA_PT_evaluation");
	strcpy(pt->label, "Evaluation");
	pt->draw= nla_panel_evaluation;
	pt->poll= nla_strip_panel_poll;
	BLI_addtail(&art->paneltypes, pt);
	
	pt= MEM_callocN(sizeof(PanelType), "spacetype nla panel modifiers");
	strcpy(pt->idname, "NLA_PT_modifiers");
	strcpy(pt->label, "Modifiers");
	pt->draw= nla_panel_modifiers;
	pt->poll= nla_strip_panel_poll;
	BLI_addtail(&art->paneltypes, pt);
}

static int nla_properties(bContext *C, wmOperator *op)
{
	ScrArea *sa= CTX_wm_area(C);
	ARegion *ar= nla_has_buttons_region(sa);
	
	if(ar) {
		ar->flag ^= RGN_FLAG_HIDDEN;
		ar->v2d.flag &= ~V2D_IS_INITIALISED; /* XXX should become hide/unhide api? */
		
		ED_area_initialize(CTX_wm_manager(C), CTX_wm_window(C), sa);
		ED_area_tag_redraw(sa);
	}
	return OPERATOR_FINISHED;
}

void NLAEDIT_OT_properties(wmOperatorType *ot)
{
	ot->name= "Properties";
	ot->idname= "NLAEDIT_OT_properties";
	
	ot->exec= nla_properties;
	ot->poll= ED_operator_nla_active;
 	
	/* flags */
	ot->flag= 0;
}
