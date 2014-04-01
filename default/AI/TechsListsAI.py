"""
The TechsListAI module provides functions that describes dependencies between
various technologies to help the AI decide which technologies should be
researched next.
"""
# TODO: further consider alternative implementations for tech priorities
# adrian_broher recommends this whole module (and related
# decision branches within ResearchAI.py) not exist-- that this 
# informations instead go in the FOCS techs.txt file as a priority expression 
# for each respective tech specification, or that it be dynamically calculated 
# by the AI scripts based purely on the info parsed from techs.txt

EXOBOT_TECH_NAME = "PRO_EXOBOTS"

def unusable_techs():
    """
    Returns a list of technologies that shouldn't be researched by the AI.
    """
    return []

def defense_techs_1():
    """
    Returns a list of technologies that implement the first planetary defensive
    measures.
    """
    return [
        "DEF_DEFENSE_NET_1",
        "DEF_GARRISON_1"
    ]

def defense_techs_2():
    """
    Returns a list of technologies that implement additional planetary defensive
    measures. To use use the AI need to have built all defense technologies that
    are provided by defense_techs_1().
    """
    return []

def tech_group_1a(): # early org_hull
    result = [
            "LRN_ALGO_ELEGANCE",
            "SHP_WEAPON_1_2",
            "GRO_PLANET_ECOL",
            "SHP_DOMESTIC_MONSTER",
            "SHP_ORG_HULL",
            "CON_ENV_ENCAPSUL",
            "LRN_ARTIF_MINDS",
            "SHP_WEAPON_1_3",
            "GRO_SUBTER_HAB",
            "SHP_WEAPON_1_4",
            "CON_ORBITAL_CON",
    ]
    return result

def tech_group_1b():  # early _lrn_artif_minds
    result = [
            "LRN_ALGO_ELEGANCE",
            "SHP_WEAPON_1_2",
            "CON_ENV_ENCAPSUL",
            "LRN_ARTIF_MINDS",
            "GRO_PLANET_ECOL",
            "SHP_DOMESTIC_MONSTER",
            "SHP_ORG_HULL",
            "GRO_SUBTER_HAB",
            "CON_ORBITAL_CON",
            "SHP_WEAPON_1_4",
    ]
    return result
    
def tech_group_2a():  # prioritizes growth & defense over weapons
    result = [
            "SHP_ZORTRIUM_PLATE",
            "DEF_DEFENSE_NET_1",
            "DEF_GARRISON_1", 
            "PRO_ROBOTIC_PROD",
            "PRO_FUSION_GEN",
            "SPY_DETECT_2",
            "GRO_SYMBIOTIC_BIO",
            "PRO_INDUSTRY_CENTER_I",
            "LRN_FORCE_FIELD",
            "SHP_WEAPON_2_1",
            "SHP_WEAPON_2_2",
            "SHP_WEAPON_2_3",
            "SHP_WEAPON_2_4",
    ]
    return result
    
def tech_group_2b():  # prioritizes weapons over growth & defense
    result = [
            "SHP_ZORTRIUM_PLATE",
            "SHP_WEAPON_2_1",
            "SHP_WEAPON_2_2",
            "SHP_WEAPON_2_3",
            "PRO_ROBOTIC_PROD",
            "PRO_FUSION_GEN",
            "DEF_DEFENSE_NET_1",
            "SHP_WEAPON_2_4",
            "PRO_INDUSTRY_CENTER_I",
            "DEF_GARRISON_1",
            "SPY_DETECT_2",
            "GRO_SYMBIOTIC_BIO",
            "LRN_FORCE_FIELD",
    ]
    return result
    
def tech_group_3a(): # without SHP_ASTEROID_REFORM
    result = [
            "GRO_GENETIC_ENG",
            "GRO_GENETIC_MED",
            "SHP_SPACE_FLUX_DRIVE",
            "PRO_SENTIENT_AUTOMATION",
            "PRO_EXOBOTS",
            "GRO_XENO_GENETICS",
            "LRN_PHYS_BRAIN",
            "LRN_TRANSLING_THT",
            "SHP_BASIC_DAM_CONT",
            "GRO_XENO_HYBRIDS",
            "DEF_DEFENSE_NET_2",
            "DEF_DEFENSE_NET_REGEN_1",
            "SHP_REINFORCED_HULL",
            "PRO_INDUSTRY_CENTER_II",
            "PRO_ORBITAL_GEN",
            "PRO_SOL_ORB_GEN",
            "DEF_GARRISON_2",
            "SHP_INTSTEL_LOG",
            "SPY_DETECT_3",
            "PRO_MICROGRAV_MAN",
            "SHP_ASTEROID_HULLS",
            "SHP_HEAVY_AST_HULL",
            "SHP_ADV_DAM_CONT",
            "LRN_QUANT_NET",
            "DEF_PLAN_BARRIER_SHLD_1",
            "DEF_DEFENSE_NET_3",
            "CON_ORBITAL_HAB",
            "CON_FRC_ENRG_STRC", 
            "DEF_PLAN_BARRIER_SHLD_2",
            "DEF_DEFENSE_NET_REGEN_2",
            "DEF_SYST_DEF_MINE_1",
            "DEF_GARRISON_3",
            "DEF_PLAN_BARRIER_SHLD_3",
            "GRO_LIFECYCLE_MAN",
            "SHP_MULTICELL_CAST",
            "SHP_ENDOCRINE_SYSTEMS",
            "SHP_CONT_BIOADAPT",
            "SHP_DEFLECTOR_SHIELD",
            "SPY_STEALTH_1",
    ]
    return result
    
def tech_group_3b():  # with SHP_ASTEROID_REFORM
    result = tech_group_3a()
    result += [ "SHP_ASTEROID_REFORM" ]
    return result
    
def tech_group_4a():  # later plasma weaps & w/o SHP_ENRG_BOUND_MAN
    result = [
            "SHP_DIAMOND_PLATE",
            "CON_NDIM_STRC", 
            "SHP_WEAPON_3_1",
            "SHP_WEAPON_3_2",
            "SHP_WEAPON_3_3",
            "SHP_WEAPON_3_4",
    ]
    return result
    
def tech_group_4b():  # faster plasma weaps & with SHP_ENRG_BOUND_MAN
    result = [
            "SHP_WEAPON_3_1",
            "SHP_WEAPON_3_2",
            "SHP_WEAPON_3_3",
            "SHP_WEAPON_3_4",
            "SHP_DIAMOND_PLATE",
            "CON_NDIM_STRC", 
            "SHP_FRC_ENRG_COMP",
            "SHP_ENRG_BOUND_MAN",
    ]
    return result

def tech_group_5():
    result = [
            "CON_CONTGRAV_ARCH",
            "LRN_XENOARCH",
            "LRN_GRAVITONICS",
            "LRN_ENCLAVE_VOID",
            "LRN_STELLAR_TOMOGRAPHY",
            "LRN_TIME_MECH",
            "DEF_PLAN_BARRIER_SHLD_4",
            "SPY_DETECT_4",
            "SHP_CONT_SYMB",
            "SHP_MONOCELL_EXP",
            "SHP_BIOADAPTIVE_SPEC",
            "SHP_SENT_HULL",
            "SHP_XENTRONIUM_PLATE",
            "GRO_CYBORG",
            "GRO_TERRAFORM",
            "GRO_ENERGY_META",
            "LRN_PSY_DOM",
            "SHP_WEAPON_4_1",
            "SHP_WEAPON_4_2",
            "SHP_WEAPON_4_3",
            "LRN_DISTRIB_THOUGHT",
            "PRO_SINGULAR_GEN",
            "SHP_PLASMA_SHIELD",
            "PRO_NEUTRONIUM_EXTRACTION",
            "CON_CONC_CAMP",
            "DEF_GARRISON_4",
            "PRO_INDUSTRY_CENTER_III",
            "SPY_STEALTH_2",
            "DEF_SYST_DEF_MINE_2",
            "DEF_SYST_DEF_MINE_3",
            "SPY_STEALTH_3",
            "SHP_WEAPON_4_4",
            "LRN_ART_BLACK_HOLE",
            "SHP_BLACKSHIELD",
            "SPY_STEALTH_4",
            "DEF_PLAN_BARRIER_SHLD_5",
            "SPY_DETECT_5",
            "GRO_GAIA_TRANS",
            "CON_ART_PLANET",
            "SHP_SOLAR_CONT",
    ]
    return result



def primary_meta_techs(index = 0):
    """
    Primary techs for all categories.
    """
    #index = 1 - index
    result = []
    
    print "Choosing Research Techlist Index %d" % index
    if index == 0:
        result =   tech_group_1a()  # early org_hull
        result += tech_group_2a()  # prioritizes growth & defense over weapons
        result += tech_group_3a()  # without SHP_ASTEROID_REFORM
        result += tech_group_4a()  # later plasma weaps & w/o SHP_ENRG_BOUND_MAN
        result += tech_group_5()    # 
    elif index == 1:
        result =   tech_group_1a()  # early _lrn_artif_minds
        result += tech_group_2a()  # prioritizes growth & defense over weapons
        result += tech_group_3a()  # without SHP_ASTEROID_REFORM
        result += tech_group_4a()  # later plasma weaps & w/o SHP_ENRG_BOUND_MAN
        result += tech_group_5()    # 
    elif index == 2:
        result =   tech_group_1a()  # early _lrn_artif_minds
        result += tech_group_2a()  # prioritizes growth & defense over weapons
        result += tech_group_3a()  # without SHP_ASTEROID_REFORM
        result += tech_group_4b()  # faster plasma weaps & with SHP_ENRG_BOUND_MAN
        result += tech_group_5()    # 
    elif index == 3:
        result =   tech_group_1a()  # early org_hull
        result += tech_group_2b()  # prioritizes weapons over growth & defense
        result += tech_group_3a()  # without SHP_ASTEROID_REFORM
        result += tech_group_4b()  # faster plasma weaps & with SHP_ENRG_BOUND_MAN
        result += tech_group_5()    # 
    elif index == 4:
        result =   tech_group_1a()  # early _lrn_artif_minds
        result += tech_group_2a()  # prioritizes growth & defense over weapons
        result += tech_group_3b()  # 3a plus SHP_ASTEROID_REFORM
        result += tech_group_4b()  # faster plasma weaps & with SHP_ENRG_BOUND_MAN
        result += tech_group_5()    # 

    return result

#the following is just for reference
#        "CON_ARCH_MONOFILS",
#        "CON_ARCH_PSYCH",
#        "CON_ART_HEAVENLY",
#        "CON_ART_PLANET",
#        "CON_ASYMP_MATS",
#        "CON_CONC_CAMP",
#        "CON_CONTGRAV_ARCH",
#        "CON_ENV_ENCAPSUL",
#        "CON_FRC_ENRG_CAMO",
#        "CON_FRC_ENRG_STRC",
#        "CON_GAL_INFRA",
#        "CON_NDIM_STRC",
#        "CON_ORBITAL_CON",
#        "CON_ORBITAL_HAB",
#        "CON_ORGANIC_STRC",
#        "CON_PLANET_DRIVE",
#        "CON_STARGATE",
#        "CON_TRANS_ARCH",
#        "DEF_DEFENSE_NET_1",
#        "DEF_DEFENSE_NET_2",
#        "DEF_DEFENSE_NET_3",
#        "DEF_DEFENSE_NET_REGEN_1",
#        "DEF_DEFENSE_NET_REGEN_2",
#        "DEF_GARRISON_1",
#        "DEF_GARRISON_2",
#        "DEF_GARRISON_3",
#        "DEF_GARRISON_4",
#        "DEF_PLANET_CLOAK",
#        "DEF_PLAN_BARRIER_SHLD_1",
#        "DEF_PLAN_BARRIER_SHLD_2",
#        "DEF_PLAN_BARRIER_SHLD_3",
#        "DEF_PLAN_BARRIER_SHLD_4",
#        "DEF_PLAN_BARRIER_SHLD_5",
#        "DEF_ROOT_DEFENSE",
#        "DEF_SYST_DEF_MINE_1",
#        "DEF_SYST_DEF_MINE_2",
#        "DEF_SYST_DEF_MINE_3",
#        "GRO_ADV_ECOMAN",
#        "GRO_BIOTERROR",
#        "GRO_CYBORG",
#        "GRO_ENERGY_META",
#        "GRO_GAIA_TRANS",
#        "GRO_GENETIC_ENG",
#        "GRO_GENETIC_MED",
#        "GRO_LIFECYCLE_MAN",
#        "GRO_NANOTECH_MED",
#        "GRO_NANO_CYBERNET",
#        "GRO_PLANET_ECOL",
#        "GRO_SUBTER_HAB",
#        "GRO_SYMBIOTIC_BIO",
#        "GRO_TERRAFORM",
#        "GRO_TRANSORG_SENT",
#        "GRO_XENO_GENETICS",
#        "GRO_XENO_HYBRIDS",
#        "LRN_ALGO_ELEGANCE",
#        "LRN_ARTIF_MINDS",
#        "LRN_ART_BLACK_HOLE",
#        "LRN_DISTRIB_THOUGHT",
#        "LRN_ENCLAVE_VOID",
#        "LRN_EVERYTHING",
#        "LRN_FORCE_FIELD",
#        "LRN_GATEWAY_VOID",
#        "LRN_GRAVITONICS",
#        "LRN_MIND_VOID",
#        "LRN_NDIM_SUBSPACE",
#        "LRN_OBSERVATORY_I",
#        "LRN_PHYS_BRAIN",
#        "LRN_PSIONICS",
#        "LRN_PSY_DOM",
#        "LRN_QUANT_NET",
#        "LRN_SPATIAL_DISTORT_GEN",
#        "LRN_STELLAR_TOMOGRAPHY",
#        "LRN_TIME_MECH",
#        "LRN_TRANSCEND",
#        "LRN_TRANSLING_THT",
#        "LRN_UNIF_CONC",
#        "LRN_XENOARCH",
#        "PRO_EXOBOTS",
#        "PRO_FUSION_GEN",
#        "PRO_INDUSTRY_CENTER_I",
#        "PRO_INDUSTRY_CENTER_II",
#        "PRO_INDUSTRY_CENTER_III",
#        "PRO_MICROGRAV_MAN",
#        "PRO_NANOTECH_PROD",
#        "PRO_NDIM_ASSMB",
#        "PRO_NEUTRONIUM_EXTRACTION",
#        "PRO_ORBITAL_GEN",
#        "PRO_ROBOTIC_PROD",
#        "PRO_SENTIENT_AUTOMATION",
#        "PRO_SINGULAR_GEN",
#        "PRO_SOL_ORB_GEN",
#        "PRO_ZERO_GEN",
#        "SHP_ADV_DAM_CONT",
#        "SHP_ANTIMATTER_TANK",
#        "SHP_ASTEROID_HULLS",
#        "SHP_ASTEROID_REFORM",
#        "SHP_BASIC_DAM_CONT",
#        "SHP_BIOADAPTIVE_SPEC",
#        "SHP_BIOTERM",
#        "SHP_BLACKSHIELD",
#        "SHP_CAMO_AST_HULL",
#        "SHP_CONTGRAV_MAINT",
#        "SHP_CONT_BIOADAPT",
#        "SHP_CONT_SYMB",
#        "SHP_DEATH_SPORE",
#        "SHP_DEFLECTOR_SHIELD",
#        "SHP_DEUTERIUM_TANK",
#        "SHP_DIAMOND_PLATE",
#        "SHP_DOMESTIC_MONSTER",
#        "SHP_ENDOCRINE_SYSTEMS",
#        "SHP_ENDOSYMB_HULL",
#        "SHP_ENRG_BOUND_MAN",
#        "SHP_FLEET_REPAIR",
#        "SHP_FRC_ENRG_COMP",
#        "SHP_GAL_EXPLO",
#        "SHP_HEAVY_AST_HULL",
#        "SHP_IMPROVED_ENGINE_COUPLINGS",
#        "SHP_INTSTEL_LOG",
#        "SHP_MASSPROP_SPEC",
#        "SHP_MIDCOMB_LOG",
#        "SHP_MIL_ROBO_CONT",
#        "SHP_MINIAST_SWARM",
#        "SHP_MONOCELL_EXP",
#        "SHP_MONOMOLEC_LATTICE",
#        "SHP_MULTICELL_CAST",
#        "SHP_MULTISPEC_SHIELD",
#        "SHP_NANOROBO_MAINT",
#        "SHP_NOVA_BOMB",
#        "SHP_N_DIMENSIONAL_ENGINE_MATRIX",
#        "SHP_ORG_HULL",
#        "SHP_PLASMA_SHIELD",
#        "SHP_QUANT_ENRG_MAG",
#        "SHP_REINFORCED_HULL",
#        "SHP_ROOT_AGGRESSION",
#        "SHP_ROOT_ARMOR",
#        "SHP_SCAT_AST_HULL",
#        "SHP_SENT_HULL",
#        "SHP_SINGULARITY_ENGINE_CORE",
#        "SHP_SOLAR_CONT",
#        "SHP_SPACE_FLUX_DRIVE",
#        "SHP_TRANSSPACE_DRIVE",
#        "SHP_WEAPON_1_2",
#        "SHP_WEAPON_1_3",
#        "SHP_WEAPON_1_4",
#        "SHP_WEAPON_2_1",
#        "SHP_WEAPON_2_2",
#        "SHP_WEAPON_2_3",
#        "SHP_WEAPON_2_4",
#        "SHP_WEAPON_3_1",
#        "SHP_WEAPON_3_2",
#        "SHP_WEAPON_3_3",
#        "SHP_WEAPON_3_4",
#        "SHP_WEAPON_4_1",
#        "SHP_WEAPON_4_2",
#        "SHP_WEAPON_4_3",
#        "SHP_WEAPON_4_4",
#        "SHP_XENTRONIUM_PLATE",
#        "SHP_ZORTRIUM_PLATE",
#        "SPY_DETECT_1",
#        "SPY_DETECT_2",
#        "SPY_DETECT_3",
#        "SPY_DETECT_4",
#        "SPY_DETECT_5",
#        "SPY_DIST_MOD",
#        "SPY_LIGHTHOUSE",
#        "SPY_PLANET_STEALTH_MOD",
#        "SPY_ROOT_DECEPTION",
#        "SPY_STEALTH_1",
#        "SPY_STEALTH_2",
#        "SPY_STEALTH_3",
#        "SPY_STEALTH_4",
