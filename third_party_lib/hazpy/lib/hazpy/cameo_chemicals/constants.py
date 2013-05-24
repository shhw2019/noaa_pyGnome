# vim:fileencoding=utf-8
# -*- coding: utf-8 -*-
"""CAMEO Chemicals data that is not in the database.
"""

import collections

# Reactivity hazard phrases
# {hazard_code : list of phrase strings}
HAZARD_PHRASES = {
   u"A1": [u"Explosive"],
   u"A2": [u"Explosive"],
   u"A3": [u"Explosive"],
   u"A4": [u"Explosive"],
   u"A5": [u"Explosive"],
   u"A6": [u"Explosive"],
   u"A8": [u"Explosive"],
   u"A9": [u"Explosive"],
   u"A10": [u"Explosive"],
   u"B1": [u"Flammable"],
   u"B3": [u"Flammable"],
   u"B4": [u"Flammable"],
   u"B5": [u"Flammable gas"],
   u"B6": [u"Flammable gas", u"Toxic gas"],
   u"C": [u"Heat generation"],
   u"D1": [u"Heat generation", u"Polymerization"],
   u"D3": [u"Toxic gas"],
   u"D4": [u"Nonflammable, nontoxic gas"],
   u"D5": [u"Combustion-enhancing gas"],
   u"D6": [u"Heat generation", u"Toxic gas", u"Corrosive"],
   u"D7": [u"Corrosive"],
   u"E": [u"Toxic"],
   u"F": [u"Unknown"],
   u"G": [u"Intense reaction"],
   u"H": [u"Radiation"],
   u"I": []	# no phrase for this code
    }

UNNA_GUIDE_NAMES = {
    # Guide number : guide name
   u"111": u"Mixed Load / Unidentified Cargo",
   u"112": u"Explosives* - Division 1.1, 1.2, 1.3, 1.5 or 1.6; Class A or B",
   u"113": u"Flammable Solids - Toxic (Wet/Desensitized Explosive)",
   u"114": u"Explosives* - Division 1.4; Class C",
   u"115": u"Gases - Flammable (Including Refrigerated Liquids)",
   u"116": u"Gases - Flammable (Unstable)",
   u"117": u"Gases - Toxic - Flammable (Extreme Hazard)",
   u"118": u"Gases - Flammable - Corrosive",
   u"119": u"Gases - Toxic - Flammable",
   u"120": u"Gases - Inert (Including Refrigerated Liquids)",
   u"121": u"Gases - Inert",
   u"122": u"Gases - Oxidizing (Including Refrigerated Liquids)",
   u"123": u"Gases - Toxic and/or Corrosive",
   u"124": u"Gases - Toxic and/or Corrosive - Oxidizing",
   u"125": u"Gases - Corrosive",
   u"126": u"Gases - Compressed or Liquefied (Including Refrigerant Gases)",
   u"127": u"Flammable Liquids (Polar / Water-Miscible)",
   u"128": u"Flammable Liquids (Non-Polar / Water-Immiscible)",
   u"129": u"Flammable Liquids (Polar / Water-Miscible / Noxious)",
   u"130": u"Flammable Liquids (Non-Polar / Water-Immiscible / Noxious)",
   u"131": u"Flammable Liquids - Toxic",
   u"132": u"Flammable Liquids - Corrosive",
   u"133": u"Flammable Solids",
   u"134": u"Flammable Solids - Toxic and/or Corrosive",
   u"135": u"Substances - Spontaneously Combustible",
   u"136": u"Substances - Spontaneously Combustible - Toxic and/or Corrosive (Air-Reactive)",
   u"137": u"Substances - Water-Reactive - Corrosive",
   u"138": u"Substances - Water-Reactive (Emitting Flammable Gases)",
   u"139": u"Substances - Water-Reactive (Emitting Flammable and Toxic Gases)",
   u"140": u"Oxidizers",
   u"141": u"Oxidizers - Toxic",
   u"142": u"Oxidizers - Toxic (Liquid)",
   u"143": u"Oxidizers (Unstable)",
   u"144": u"Oxidizers (Water-Reactive)",
   u"145": u"Organic Peroxides (Heat and Contamination Sensitive)",
   u"146": u"Organic Peroxides (Heat, Contamination and Friction Sensitive)",
   u"147": u"Lithium Ion Batteries",
   u"148": u"Organic Peroxides (Heat and Contamination Sensitive / Temperature Controlled)",
   u"149": u"Substances (Self-Reactive)",
   u"150": u"Substances (Self-Reactive / Temperature Controlled)",
   u"151": u"Substances - Toxic (Non-Combustible)",
   u"152": u"Substances - Toxic (Combustible)",
   u"153": u"Substances - Toxic and/or Corrosive (Combustible)",
   u"154": u"Substances - Toxic and/or Corrosive (Non-Combustible)",
   u"155": u"Substances - Toxic and/or Corrosive (Flammable / Water-Sensitive)",
   u"156": u"Substances - Toxic and/or Corrosive (Combustible / Water-Sensitive)",
   u"157": u"Substances - Toxic and/or Corrosive (Non-Combustible / Water-Sensitive)",
   u"158": u"Infectious Substances",
   u"159": u"Substances (Irritating)",
   u"160": u"Halogenated Solvents",
   u"161": u"Radioactive Materials (Low Level Radiation)",
   u"162": u"Radioactive Materials (Low to Moderate Level Radiation)",
   u"163": u"Radioactive Materials (Low to High Level Radiation)",
   u"164": u"Radioactive Materials (Special Form / Low to High Level External Radiation)",
   u"165": u"Radioactive Materials (Fissile / Low to High Level Radiation)",
   u"166": u"Radioactive Materials - Corrosive (Uranium Hexafluoride / Water-Sensitive)",
   u"167": u"Fluorine (Refrigerated Liquid)",
   u"168": u"Carbon Monoxide (Refrigerated Liquid)",
   u"169": u"Aluminum (Molten)",
   u"170": u"Metals (Powders, Dusts, Shavings, Borings, Turnings, or Cuttings, Etc.)",
   u"171": u"Substances (Low to Moderate Hazard)",
   u"172": u"Gallium and Mercury"
    }

UNNA_LABEL_NAMES = {
    # Label number (string) : label name 
   u"1.1": u"1.1 - Explosives (with a mass explosion hazard)",
   u"1.2": u"1.2 - Explosives (with a projection hazard)",
   u"1.3": u"1.3 - Explosives (with predominately a fire hazard)",
   u"1.4": u"1.4 - Explosives (with no significant blast hazard)",
   u"1.5": u"1.5 - Very insensitive explosives; blasting agents",
   u"1.6": u"1.6 - Extremely insensitive detonating substances",
   u"2.1": u"2.1 - Flammable gas",
   u"2.2": u"2.2 - Non-flammable compressed gas",
   u"2.2_Ox": u"2.2 - Oxygen",
   u"2.3": u"2.3 - Poisonous gas",
   u"3": u"3 - Flammable liquid",
   u"3.0_CL": u"3 - Combustible liquid (U.S.)",
   u"4.1": u"4.1 - Flammable solid",
   u"4.2": u"4.2 - Spontaneously combustible material",
   u"4.3": u"4.3 - Dangerous when wet material",
   u"5.1": u"5.1 - Oxidizer",
   u"5.2": u"5.2 - Organic peroxide",
   u"6.1": u"6.1 - Poisonous materials",
   u"6.2": u"6.2 - Infectious substances (Etiologic agent)",
   u"7": u"7 - Radioactive material",
   u"8": u"8 - Corrosive material",
   u"9": u"9 - Miscellaneous hazardous material",
   u"1005": u"UN/NA number 1005 (Ammonia, Anhydrous)",
   u"3373": u"UN/NA number 3373",
    }

UNNA_EXPLOSIVE_CODES = {
    # Code (string) : title
   u"A": u"Substances which are expected to mass detonate very soon after fire reaches them.",
   u"B": u"Articles which are expected to mass detonate very soon after fire reaches them.",
   u"C": u"Substances or articles which may be readily ignited and burn violently without necessarily exploding.",
   u"D": u"Substances or articles which may mass detonate (with blast and/or fragment hazard) when exposed to fire.",
   u"E": u"Articles which may mass detonate in a fire.",
   u"F": u"Articles which may mass detonate in a fire.",
   u"G": u"Substances and articles which may mass explode and give off smoke or toxic gases.",
   u"H": u"Articles which in a fire may eject hazardous projectiles and dense white smoke.",
   u"J": u"Articles which may mass explode.",
   u"K": u"Articles which in a fire may eject hazardous projectiles and toxic gases.",
   u"L": u"Substances and articles which present a special risk and could be activated by exposure to air or water.",
   u"N": u"Articles which contain only extremely insensitive detonating substances and demonstrate a negligible probability of accidental ignition or propagation.",
   u"S": u"Packaged substances or articles which, if accidently initiated, produce effects that are usually confined to the immediate vicinity."
    }

DUPONT_FABRICS = collections.OrderedDict([
    (u"QC", u"Tychem® QC"),
    (u"SL", u"Tychem® SL"),
    (u"TF", u"Tychem® F"),
    (u"TP", u"Tychem® ThermoPro"),
    (u"C3", u"Tychem® CPF 3"),
    (u"BR", u"Tychem® BR"),
    (u"LV", u"Tychem® LV"),
    (u"RC", u"Tychem® Responder® CSM"),
    (u"TK", u"Tychem® TK"),
    (u"RF", u"Tychem® Reflector®"),
    ])
