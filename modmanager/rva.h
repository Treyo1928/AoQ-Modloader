#pragma once

/* ======================================================================
 * RVA DEFINITIONS  (all from dump.cs, AoQ 0.5.0)
 * ====================================================================== */

/* Hook targets */
#define RVA_StartMenu_Start              0x4813B0
#define RVA_OVRCameraRig_UpdateAnchors   0x5AB168
#define RVA_Button_Press                 0x6AD87C

/* UnityEngine.Component */
#define RVA_Component_get_transform      0xC6A29C
#define RVA_Component_get_gameObject     0xC6A2EC
#define RVA_Component_GetComponent       0xC6A33C
#define RVA_Component_GetCompInChildren  0xC6A49C  /* GetComponentInChildren(Type,bool) */
#define RVA_Component_GetComponentsInCh  0xC6A5AC  /* GetComponentsInChildren(Type,bool) */

/* UnityEngine.GameObject */
#define RVA_GO_get_transform             0xC744F8
#define RVA_GO_SetActive                 0xC745F0

/* UnityEngine.Object */
#define RVA_Object_get_name              0xCE6070
#define RVA_Object_set_name              0xCEB33C
#define RVA_Object_Instantiate           0xCEBA34
#define RVA_Object_Instantiate_Parent    0xCEBBB0  /* Instantiate(Object,Transform,bool) */
#define RVA_Object_DontDestroyOnLoad     0xCEC014
#define RVA_Object_FindObjectsOfType     0xCEBFC4

/* UnityEngine.Transform */
#define RVA_Transform_SetParent_bool     0xFD6444
#define RVA_Transform_set_localPosition  0xFD51E8
#define RVA_Transform_get_localScale     0xFD6010
#define RVA_Transform_set_localScale     0xFD60E8
#define RVA_Transform_get_parent         0xFD61A0
#define RVA_Transform_get_childCount     0xFD7708
#define RVA_Transform_GetChild           0xFD7B6C
#define RVA_Transform_Find               0xFD7808

/* UnityEngine.RectTransform */
#define RVA_RectTransform_get_anchoredPos 0xCF3568
#define RVA_RectTransform_set_anchoredPos 0xCF3634

/* UnityEngine.UI.Selectable */
#define RVA_Selectable_set_interactable   0xB22264

/* UnityEngine.Canvas */
#define RVA_Canvas_get_renderMode        0x10E3208
#define RVA_Canvas_set_renderMode        0x10E3258
#define RVA_Canvas_get_worldCamera       0x10E3888
#define RVA_Canvas_set_worldCamera       0x10E38D8

/* UnityEngine.Renderer */
#define RVA_Renderer_get_sharedMaterial  0xCF8104
#define RVA_Renderer_set_sharedMaterial  0xCF8154

/* UnityEngine.UI.Graphic */
#define RVA_Graphic_SetAllDirty          0xA157E8
#define RVA_Graphic_set_color            0xA156A4

/* UnityEngine.GameObject */
#define RVA_GameObject_ctor_str          0xC74738
#define RVA_GameObject_AddComponent      0xC744A0

/* UnityEngine.RectTransform */
#define RVA_RectTransform_set_anchorMin  0xCF332C
#define RVA_RectTransform_set_anchorMax  0xCF34B0
#define RVA_RectTransform_set_offsetMin  0xCF3BBC
#define RVA_RectTransform_set_offsetMax  0xCF410C

/* TMPro.TextMeshProUGUI */
#define RVA_TMP_SetAllDirty              0x9D46D4

/* UnityEngine.Shader */
#define RVA_Shader_Find                  0xFCE400

/* UnityEngine.Material */
#define RVA_Material_get_shader          0xC786F8
#define RVA_Material_set_shader          0xC78748
#define RVA_Material_EnableKeyword       0xC795C0

/* TMPro.TMP_Text — fontSharedMaterial */
#define RVA_TMP_get_sharedMaterial       0x43ACB0
#define RVA_TMP_set_sharedMaterial       0x43ACB8
#define RVA_TMP_get_text                 0x43AAC0
#define RVA_TMP_set_text                 0x43AAC8

/* OVRInput */
#define RVA_OVRInput_GetDown             0xAF7CAC
#define RVA_OVRInput_GetUp               0xAF7F80

/* OVRInput.Button enum values */
#define OVR_BTN_ONE          0x00000001  /* A (right) / X (left) */
#define OVR_BTN_TWO          0x00000002  /* B (right) / Y (left) */
#define OVR_BTN_PRIMARY_TS_U 0x00000400  /* Primary thumbstick up   */
#define OVR_BTN_PRIMARY_TS_D 0x00000800  /* Primary thumbstick down */
#define OVR_CTRL_ACTIVE      ((int)0x80000000)

/* UnityEngine.AssetBundle */
#define RVA_AssetBundle_LoadFromFileAsync 0x10F3C54
#define RVA_AssetBundle_LoadAssetAsync    0x10F3D10
#define RVA_ABCreateReq_get_assetBundle   0x10F41E0
#define RVA_ABRequest_get_asset           0x10F4238
#define RVA_AsyncOp_get_isDone            0xC63AC4
