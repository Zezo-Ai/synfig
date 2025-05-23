"""
Will store all the functions corresponding to text layer in lottie
"""

import sys
import settings
from common.Count import Count
from helpers.transform import gen_helpers_transform
from helpers.blendMode import get_blend
from common.misc import is_animated
from properties.value import gen_properties_value
from properties.valueKeyframed import gen_value_Keyframed
from common.misc import get_frame, parse_position
sys.path.append("..")

def calc_font_data(lottie,layer):
    """
    Calculates the font data used in the outermost canvas

    Args:
        lottie (dict)       : Outermost dictionary used in Lottie file
        layer  (common.Layer.Layer) : Synfig format text layer

    Returns:
        (None)
    """
    default_list = {
        "origin": 0,
        "fPath": "",
        "fClass": "",
        "fFamily": "",
        "fWeight": "",
        "fName": "",
        "ascent": 75.9994506835938
      }
    #Ascent is not documentated in the Lottie documentation and it's purpose is not known yet.

    style_dict = {0:'Regular', 1:'Regular', 2:'Italic'}
    #Lottie does not support oblique style of Synfig, so defaulting to Regular Style for it


    default_list.update({'fStyle'  : style_dict[int(layer.get_param("style").get()[0].attrib["value"])]})
    default_list.update({'fWeight' : layer.get_param("weight").get()[0].attrib["value"]})
    font = layer.get_param("family").get()[0].text
    if font in settings.FONT_STYLES:
	    if "Comic" in font:
	    	default_list["fFamily"] = "Comic Sans MS"
	    	default_list["fName"]   = "ComicSansMS"
	    else:
	    	default_list["fFamily"] = font
	    	default_list["fName"]   = "".join(font.split())

    lottie["fonts"]["list"].append(default_list)

def calc_default_text_properties(layer):
    """
    Generated the lottie dictionary corresponding to properties which won't change in text-animation

    Args:
        layer  (common.Layer.Layer) : Synfig format text layer

    Returns:
        Lottie format default dictionary for text animation properties
    """
    default_dict = {
      "nm": "",
      "s": {
        "t" : 0,
        "xe": {},
        "ne": {},
        "a" : {},
        "b" : 1,
        "rn": 0,
        "sh": 1,
        "r" : 1
      }
    }
    gen_properties_value(default_dict["s"]["xe"],
                             0,
                             7,
                             settings.DEFAULT_ANIMATED,
                             settings.NO_INFO)
    gen_properties_value(default_dict["s"]["ne"],
                             0,
                             8,
                             settings.DEFAULT_ANIMATED,
                             settings.NO_INFO)
    gen_properties_value(default_dict["s"]["a"],
                             100,
                             4,
                             settings.DEFAULT_ANIMATED,
                             settings.NO_INFO)

    default_dict["nm"] = layer.get_description()

    return default_dict

def calc_anchor_alignment(lottie):
    """
    Generates the dictionary corresponding to the anchor alignment properties

    Args:
        lottie (dict) : Lottie generated text layer stored here
    """
    lottie["t"]["m"] = {
                          "g": 1,
                          "a": {}
                       } #Has no corresponding synfig property, so using default values
    gen_properties_value(lottie["t"]["m"]["a"],
                             [0,0],
                             2,
                             settings.DEFAULT_ANIMATED,
                             settings.NO_INFO)

def calculate_text_animation(lottie,animated):
    """
    Generates the dictionary corresponding to text properties

    Args:
        lottie  (dict)                 : Lottie generated text properties dictionary
        animated (lxml.etree._Element) : Synfig format animation
    """

    waypoints = len(animated)
    #Adding text to each waypoint
    for waypoint in range(waypoints):
        temp  = {
              "s": {
                "s": 50,
                "f": "",
                "t": "",
                "j": 2,
                "tr": 0,
                "lh": 50,
                "ls": 1,
                "fc": [1,1,1,1]
              },
              "t": 0
            }
        cur_pos = parse_position(animated, waypoint)
        text_val = cur_pos.get_val()
        time = get_frame(animated[waypoint])
        ax = text_val[0].split("\n")
        final_text = "\r".join(ax)
        temp["s"]["t"] = final_text
        temp["t"] = time
        lottie.append(temp)

def gen_layer_text(lottie, layer, idx):
    """
    Generates the dictionary corresponding to layers/text.json

    Args:
        lottie (dict)       : Lottie generated text layer stored here
        layer  (common.Layer.Layer) : Synfig format text layer
        idx    (int)        : Stores the index(number of) of text layer

    Returns:
        (None)
    """
    index = Count()
    lottie["ddd"] = settings.DEFAULT_3D
    lottie["ind"] = idx+1
    lottie["ty"] = settings.LAYER_TEXT_TYPE
    lottie["nm"] = layer.get_description()
    lottie["sr"] = settings.LAYER_DEFAULT_STRETCH
    lottie["ks"] = {}   # Transform properties to be filled

    calc_font_data(settings.lottie_format,layer)

    lottie["ao"] = settings.LAYER_DEFAULT_AUTO_ORIENT
    lottie["ip"] = settings.lottie_format["ip"]
    lottie["op"] = settings.lottie_format["op"]
    lottie["st"] = 0            # Don't know yet
    lottie["t"] = {}
    calc_anchor_alignment(lottie)
    lottie["t"]["a"] = []

    origin = layer.get_param("origin")
    origin.animate("vector")

    anchor = layer.get_param("orient")
    anchor.animate("vector",True)

    scale = layer.get_param("size")
    scale.animate("stretch_layer_scale")
    pos = origin
    pos.animate("vector",True)

    gen_helpers_transform(lottie["ks"], pos, anchor, scale)

    lottie["t"]["a"].append(calc_default_text_properties(layer))
    lottie["t"]["a"][0]["a"] = {"fc":{},"o":{},"t":{}}

    color = layer.get_param("color")
    color.animate("color")
    color.fill_path(lottie["t"]["a"][0]["a"],"fc")

    opacity = layer.get_param("amount").get()
    is_animate = is_animated(opacity[0])
    if is_animate == settings.ANIMATED:
        # Telling the function that this is for opacity
        opacity[0].attrib['type'] = 'opacity'
        gen_value_Keyframed(lottie["t"]["a"][0]["a"]["o"], opacity[0], index.inc())

    else:
        if is_animate == settings.NOT_ANIMATED:
            val = float(opacity[0].attrib["value"])
        else:
            val = float(opacity[0][0][0].attrib["value"])
        val = val * settings.OPACITY_CONSTANT
        gen_properties_value(lottie["t"]["a"][0]["a"]["o"],
                             val,
                             index.inc(),
                             settings.DEFAULT_ANIMATED,
                             settings.NO_INFO)

    tracking = layer.get_param("compress").get()
    is_animate = is_animated(tracking[0])
    if is_animate == settings.ANIMATED:
        # Telling the function that this is for separation
        tracking[0].attrib['type'] = 'separation'
        gen_value_Keyframed(lottie["t"]["a"][0]["a"]["t"], tracking[0], index.inc())

    else:
        if is_animate == settings.NOT_ANIMATED:
            val = float(tracking[0].attrib["value"])
        else:
            val = float(tracking[0][0][0].attrib["value"])
        if val == 1.0:
            val = (val - 0.45) * settings.SEPARATION_CONSTANT
        else:
            val = val * settings.SEPARATION_CONSTANT
        gen_properties_value(lottie["t"]["a"][0]["a"]["t"],
                             val,
                             index.inc(),
                             settings.DEFAULT_ANIMATED,
                             settings.NO_INFO)

    lottie["t"]["d"] = {}
    lottie["t"]["p"] = {}
    lottie["t"]["d"]["k"] = []
    default = {
              "s": {
                "s": 50,
                "f": "",
                "t": "",
                "j": 2,
                "tr": 0,
                "lh": 50,
                "ls": 1,
                "fc": [1,1,1,1]
              },
              "t": 0
            }

    texts = layer.get_param("text").get()
    is_animate = is_animated(texts[0])
    if is_animate == settings.ANIMATED:
        texts[0].attrib['type'] = 'text'
        calculate_text_animation(lottie["t"]["d"]["k"],texts[0])
    else:
        ax = layer.get_param("text").get()[0].text.split("\n")
        #Line separation is not supported so adding separation only for integer values
        line_separation = layer.get_param("vcompress").get()[0].attrib["value"]
        breaks = "\r"*round(float(line_separation))
        text = breaks.join(ax)
        default["s"]["t"] = text
        lottie["t"]["d"]["k"].append(default)

    get_blend(lottie, layer)

