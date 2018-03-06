/*
 * Generated by asn1c-0.9.29 (http://lionet.info/asn1c)
 * From ASN.1 module "RRLP-Components"
 * 	found in "../ulp.asn1"
 */

#include "GPSTimeAssistanceMeasurements.h"

static int
memb_referenceFrameMSB_constraint_1(const asn_TYPE_descriptor_t *td, const void *sptr,
			asn_app_constraint_failed_f *ctfailcb, void *app_key) {
	long value;
	
	if(!sptr) {
		ASN__CTFAIL(app_key, td, sptr,
			"%s: value not given (%s:%d)",
			td->name, __FILE__, __LINE__);
		return -1;
	}
	
	value = *(const long *)sptr;
	
	if((value >= 0 && value <= 63)) {
		/* Constraint check succeeded */
		return 0;
	} else {
		ASN__CTFAIL(app_key, td, sptr,
			"%s: constraint failed (%s:%d)",
			td->name, __FILE__, __LINE__);
		return -1;
	}
}

static int
memb_gpsTowSubms_constraint_1(const asn_TYPE_descriptor_t *td, const void *sptr,
			asn_app_constraint_failed_f *ctfailcb, void *app_key) {
	long value;
	
	if(!sptr) {
		ASN__CTFAIL(app_key, td, sptr,
			"%s: value not given (%s:%d)",
			td->name, __FILE__, __LINE__);
		return -1;
	}
	
	value = *(const long *)sptr;
	
	if((value >= 0 && value <= 9999)) {
		/* Constraint check succeeded */
		return 0;
	} else {
		ASN__CTFAIL(app_key, td, sptr,
			"%s: constraint failed (%s:%d)",
			td->name, __FILE__, __LINE__);
		return -1;
	}
}

static int
memb_deltaTow_constraint_1(const asn_TYPE_descriptor_t *td, const void *sptr,
			asn_app_constraint_failed_f *ctfailcb, void *app_key) {
	long value;
	
	if(!sptr) {
		ASN__CTFAIL(app_key, td, sptr,
			"%s: value not given (%s:%d)",
			td->name, __FILE__, __LINE__);
		return -1;
	}
	
	value = *(const long *)sptr;
	
	if((value >= 0 && value <= 127)) {
		/* Constraint check succeeded */
		return 0;
	} else {
		ASN__CTFAIL(app_key, td, sptr,
			"%s: constraint failed (%s:%d)",
			td->name, __FILE__, __LINE__);
		return -1;
	}
}

static asn_oer_constraints_t asn_OER_memb_referenceFrameMSB_constr_2 CC_NOTUSED = {
	{ 1, 1 }	/* (0..63) */,
	-1};
static asn_per_constraints_t asn_PER_memb_referenceFrameMSB_constr_2 CC_NOTUSED = {
	{ APC_CONSTRAINED,	 6,  6,  0,  63 }	/* (0..63) */,
	{ APC_UNCONSTRAINED,	-1, -1,  0,  0 },
	0, 0	/* No PER value map */
};
static asn_oer_constraints_t asn_OER_memb_gpsTowSubms_constr_3 CC_NOTUSED = {
	{ 2, 1 }	/* (0..9999) */,
	-1};
static asn_per_constraints_t asn_PER_memb_gpsTowSubms_constr_3 CC_NOTUSED = {
	{ APC_CONSTRAINED,	 14,  14,  0,  9999 }	/* (0..9999) */,
	{ APC_UNCONSTRAINED,	-1, -1,  0,  0 },
	0, 0	/* No PER value map */
};
static asn_oer_constraints_t asn_OER_memb_deltaTow_constr_4 CC_NOTUSED = {
	{ 1, 1 }	/* (0..127) */,
	-1};
static asn_per_constraints_t asn_PER_memb_deltaTow_constr_4 CC_NOTUSED = {
	{ APC_CONSTRAINED,	 7,  7,  0,  127 }	/* (0..127) */,
	{ APC_UNCONSTRAINED,	-1, -1,  0,  0 },
	0, 0	/* No PER value map */
};
asn_TYPE_member_t asn_MBR_GPSTimeAssistanceMeasurements_1[] = {
	{ ATF_NOFLAGS, 0, offsetof(struct GPSTimeAssistanceMeasurements, referenceFrameMSB),
		(ASN_TAG_CLASS_CONTEXT | (0 << 2)),
		-1,	/* IMPLICIT tag at current level */
		&asn_DEF_NativeInteger,
		0,
		{ &asn_OER_memb_referenceFrameMSB_constr_2, &asn_PER_memb_referenceFrameMSB_constr_2,  memb_referenceFrameMSB_constraint_1 },
		0, 0, /* No default value */
		"referenceFrameMSB"
		},
	{ ATF_POINTER, 3, offsetof(struct GPSTimeAssistanceMeasurements, gpsTowSubms),
		(ASN_TAG_CLASS_CONTEXT | (1 << 2)),
		-1,	/* IMPLICIT tag at current level */
		&asn_DEF_NativeInteger,
		0,
		{ &asn_OER_memb_gpsTowSubms_constr_3, &asn_PER_memb_gpsTowSubms_constr_3,  memb_gpsTowSubms_constraint_1 },
		0, 0, /* No default value */
		"gpsTowSubms"
		},
	{ ATF_POINTER, 2, offsetof(struct GPSTimeAssistanceMeasurements, deltaTow),
		(ASN_TAG_CLASS_CONTEXT | (2 << 2)),
		-1,	/* IMPLICIT tag at current level */
		&asn_DEF_NativeInteger,
		0,
		{ &asn_OER_memb_deltaTow_constr_4, &asn_PER_memb_deltaTow_constr_4,  memb_deltaTow_constraint_1 },
		0, 0, /* No default value */
		"deltaTow"
		},
	{ ATF_POINTER, 1, offsetof(struct GPSTimeAssistanceMeasurements, gpsReferenceTimeUncertainty),
		(ASN_TAG_CLASS_CONTEXT | (3 << 2)),
		-1,	/* IMPLICIT tag at current level */
		&asn_DEF_GPSReferenceTimeUncertainty,
		0,
		{ 0, 0, 0 },
		0, 0, /* No default value */
		"gpsReferenceTimeUncertainty"
		},
};
static const int asn_MAP_GPSTimeAssistanceMeasurements_oms_1[] = { 1, 2, 3 };
static const ber_tlv_tag_t asn_DEF_GPSTimeAssistanceMeasurements_tags_1[] = {
	(ASN_TAG_CLASS_UNIVERSAL | (16 << 2))
};
static const asn_TYPE_tag2member_t asn_MAP_GPSTimeAssistanceMeasurements_tag2el_1[] = {
    { (ASN_TAG_CLASS_CONTEXT | (0 << 2)), 0, 0, 0 }, /* referenceFrameMSB */
    { (ASN_TAG_CLASS_CONTEXT | (1 << 2)), 1, 0, 0 }, /* gpsTowSubms */
    { (ASN_TAG_CLASS_CONTEXT | (2 << 2)), 2, 0, 0 }, /* deltaTow */
    { (ASN_TAG_CLASS_CONTEXT | (3 << 2)), 3, 0, 0 } /* gpsReferenceTimeUncertainty */
};
asn_SEQUENCE_specifics_t asn_SPC_GPSTimeAssistanceMeasurements_specs_1 = {
	sizeof(struct GPSTimeAssistanceMeasurements),
	offsetof(struct GPSTimeAssistanceMeasurements, _asn_ctx),
	asn_MAP_GPSTimeAssistanceMeasurements_tag2el_1,
	4,	/* Count of tags in the map */
	asn_MAP_GPSTimeAssistanceMeasurements_oms_1,	/* Optional members */
	3, 0,	/* Root/Additions */
	-1,	/* First extension addition */
};
asn_TYPE_descriptor_t asn_DEF_GPSTimeAssistanceMeasurements = {
	"GPSTimeAssistanceMeasurements",
	"GPSTimeAssistanceMeasurements",
	&asn_OP_SEQUENCE,
	asn_DEF_GPSTimeAssistanceMeasurements_tags_1,
	sizeof(asn_DEF_GPSTimeAssistanceMeasurements_tags_1)
		/sizeof(asn_DEF_GPSTimeAssistanceMeasurements_tags_1[0]), /* 1 */
	asn_DEF_GPSTimeAssistanceMeasurements_tags_1,	/* Same as above */
	sizeof(asn_DEF_GPSTimeAssistanceMeasurements_tags_1)
		/sizeof(asn_DEF_GPSTimeAssistanceMeasurements_tags_1[0]), /* 1 */
	{ 0, 0, SEQUENCE_constraint },
	asn_MBR_GPSTimeAssistanceMeasurements_1,
	4,	/* Elements count */
	&asn_SPC_GPSTimeAssistanceMeasurements_specs_1	/* Additional specs */
};
