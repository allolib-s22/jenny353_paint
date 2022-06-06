#include <stdlib.h> 
#include <iostream>
#include <fstream>


#include "al/app/al_App.hpp"
#include "al/graphics/al_Shapes.hpp"
#include "al/math/al_Random.hpp"
#include "al/math/al_Ray.hpp"

#include "Gamma/Analysis.h"
#include "Gamma/Effects.h"
#include "Gamma/Envelope.h"
#include "Gamma/Oscillator.h"
#include "Gamma/Domain.h"

#include "al/scene/al_PolySynth.hpp"
#include "al/scene/al_SynthSequencer.hpp"
#include "al/ui/al_ControlGUI.hpp"
#include "al/ui/al_Parameter.hpp"
#include "al/io/al_Window.hpp"
#include "al/system/al_Time.hpp"
#include "al/ui/al_PresetSequencer.hpp"


#include <vector>


using namespace al;

class BrushStroke {        
  public:         
    //vector of Dots < vet 3d positions, float timeFromStart > ,  each dot has a position, and a time from the stroke start
    //  keeps track of onMouseDrag dots that are drawn and when to draw them
    std::vector <std::pair<Vec3f, float>> dots; 
    //color of stroke
    Color color{1.f, 1.f, 1.f, 1.f};


};