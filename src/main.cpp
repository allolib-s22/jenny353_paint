/*
Allocore 

Description:
Interact with paintbrush using ray intersection tests, and draw 3D spheres that are mapped sonically to 2D coordinates

*/

#include "al/app/al_App.hpp"
#include "al/graphics/al_Shapes.hpp"
#include "al/math/al_Random.hpp"
#include "al/math/al_Ray.hpp"

#include "Gamma/Analysis.h"
#include "Gamma/Effects.h"
#include "Gamma/Envelope.h"
#include "Gamma/Oscillator.h"

#include "al/scene/al_PolySynth.hpp"
#include "al/scene/al_SynthSequencer.hpp"
#include "al/ui/al_ControlGUI.hpp"
#include "al/ui/al_Parameter.hpp"
#include "al/io/al_Window.hpp"
#include "al/system/al_Time.hpp"


#include <vector>
#include "sound.hpp"

using namespace al;


struct RayBrush : App {

  //variables for graphics
  Material material;
  Light light;
  Mesh mMesh;

  std::vector <Vec3f> pos;
  std::vector <Color> colorSpheres;
  Color colorPicker{1, 1, 1};
  bool move_with_mouse = false;
  bool loop = false;

  std::vector <int> midiNotes; // keeps track of the notes for each stroke
  std::vector <float> durations; // keeps track of the durations for each stroke
  std::vector <int> start_stroke_positions; // keeps track of where all the stroke positions start for the undo function 

  //variables for sound
  SynthGUIManager<Sound> synthManager{"Sound"};
  int midiNote = 0;
  float frequency = 0;
  Timer durationTimer;

  void onCreate() override {
    //for graphics
    nav().pos(0, 0, 80); //zoom in and out, higher z is out farther away
    light.pos(0, 0, 80); // where the light is set
    addSphere(mMesh, 0.4); 
    mMesh.generateNormals();

    //for sound
    // disable nav control mouse drag to look
    navControl().useMouse(false);

    // Set sampling rate for Gamma objects from app's audio
    gam::sampleRate(audioIO().framesPerSecond());
    imguiInit();
    synthManager.synthRecorder().verbose(true);
  }

  // The audio callback function. Called when audio hardware requires data
  void onSound(AudioIOData& io) override {
    synthManager.render(io);  // Render audio
  }

  void onAnimate(double dt) override {
    // The GUI is prepared here
   
    //navControl().useMouse(!isImguiUsingInput()); //allows screen to move with mouse
    //navControl().active(!isImguiUsingInput());  // not sure what this does
    
    imguiBeginFrame();
    ImGui::Begin("my window");
    // Draw a window that contains the synth control panel
    synthManager.drawSynthControlPanel();
    
    // Edit 3 floats representing a color
    ImGui::ColorEdit3("Color", colorPicker.components);
    ImGui::Checkbox("Move With Mouse", &move_with_mouse);
    if (move_with_mouse) {
      navControl().useMouse(!isImguiUsingInput()); //allows screen to move with mouse
    }
    ImGui::Checkbox("Turn on Loop", &loop); //turn on loop for note to keep repeating after being drawn 
    if(loop){
      double duration = durations.back(); //get duration
      const float A4 = 220.f;
      int noteToLoop = midiNotes.back();
      //keep playing notes for a certain duration each time until loop is checked off
      synthManager.voice()->setInternalParameterValue(
            "frequency", ::pow(2.f, (noteToLoop - 69.f) / 12.f) * A4);
      synthManager.triggerOn(noteToLoop);
      synthManager.synthSequencer().addVoice(synthManager.voice(), 0.f, duration);
    }

    if (ImGui::Button("Clear Drawing")) {
        // Buttons return true when clicked
        // clears the screen
      pos.clear();
      colorSpheres.clear(); 
    }
    if (ImGui::Button("Undo")) {
        // undoes the most recent stroke from last on mouse down
        // erase the last elements from last_stroke_position to end:
        int getPrevStroke = start_stroke_positions.back();
        start_stroke_positions.pop_back();
        pos.erase(pos.begin() + getPrevStroke, pos.end());
        colorSpheres.erase(colorSpheres.begin() + getPrevStroke, colorSpheres.end());
        midiNotes.pop_back(); //remove associated midinote

    }
    ImGui::End();
    imguiEndFrame();
  }


  virtual void onDraw(Graphics &g) override {
    g.clear(0);
    gl::depthTesting(true);
    g.lighting(true);
    //std::cout<<"in onDraw "<< " pos[0]: "<< pos[0] <<std::endl;
    //draw and color spheres 
    for (int i = 0; i < pos.size(); i++) {
      //std::cout<<"in onDraw, i = " << i << " pos: "<< pos[i] << "color = " <<colorSpheres[i].y<<std::endl;
      g.pushMatrix();
      g.translate(pos[i]);
      //g.color(1, .87, .5);
      g.color(colorSpheres[i]);
      g.draw(mMesh);
      g.popMatrix();
    }
    // Render the synth's graphics
    synthManager.render(g);
    // GUI is drawn here
    imguiDraw();
    //change z coord with gui
    nav().pos(0, 0, synthManager.voice()->getInternalParameterValue("z")); //zoom in and out, higher z is out farther away
    light.pos(0, 0, synthManager.voice()->getInternalParameterValue("z")); // where the light is set
  }

  Vec3d unproject(Vec3d screenPos) {
    auto &g = graphics();
    auto mvp = g.projMatrix() * g.viewMatrix() * g.modelMatrix();
    Matrix4d invprojview = Matrix4d::inverse(mvp);
    Vec4d worldPos4 = invprojview.transform(screenPos);
    return worldPos4.sub<3>(0) / worldPos4.w;
  }


  bool onMouseDown(const Mouse &m) override {
    // if mouse clicks on image gui, do not draw!!!
    if(isImguiUsingInput()){
      return true;
    }
    //start duration timer
    durationTimer.start();

    Vec3d screenPos;
    screenPos.x = (m.x() * 1. / width()) * 2. - 1.;
    screenPos.y = ((height() - m.y()) * 1. / height()) * 2. - 1.;
    screenPos.z = 1.;
    Vec3d worldPos = unproject(screenPos);
    //add a sphere to plane
    Vec3f position = Vec3f(worldPos.x, worldPos.y, worldPos.z);
    //std::cout<<"postion = "<< position <<std::endl;

    start_stroke_positions.push_back(pos.size()); //set the last stroke positon to start from here
    pos.push_back(position);


    //trigger note on
    //set x and y coord on gui
    synthManager.voice()->setInternalParameterValue("x",m.x());
    synthManager.voice()->setInternalParameterValue("y",m.y());

    // trigger note on
    const float A4 = 220.f;
    midiNote = (m.x()+m.y())%87 + 21; //(range from 21-108)
    std::cout<<"Drawing midi note = "<< midiNote <<std::endl;
    //std::cout<<"currentSphereCount = "<< currentSphereCount <<std::endl;

    if (midiNote > 0) {
        synthManager.voice()->setInternalParameterValue(
            "frequency", ::pow(2.f, (midiNote - 69.f) / 12.f) * A4);
        synthManager.triggerOn(midiNote);
      }
    Color sphereColor = colorPicker;
    colorSpheres.push_back(sphereColor);
    midiNotes.push_back(midiNote);
    
    return true;
  }
  bool onMouseDrag(const Mouse &m) override {
    // if mouse drags on image gui, do not draw!!!
    if(isImguiUsingInput()){
      return true;
    }

    Vec3d screenPos;
    screenPos.x = (m.x() * 1. / width()) * 2. - 1.;
    screenPos.y = ((height() - m.y()) * 1. / height()) * 2. - 1.;
    screenPos.z = 1.;
    Vec3d worldPos = unproject(screenPos);
    //add a sphere to plane
    Vec3f position = Vec3f(worldPos.x, worldPos.y, worldPos.z);

    pos.push_back(position);
    Color sphereColor = colorPicker;
    colorSpheres.push_back(sphereColor);
  
    return true;
  }

  bool onMouseUp(const Mouse &m) override {
    durationTimer.stop();
    durations.push_back(durationTimer.elapsedSec());
    //trigger note off
    synthManager.triggerOff(midiNote);

    return true;
  }

  //use sequencncer->synthManager()
  //count duration of mouse sequence and add duration with voice after to make chords
};
int main() {
  RayBrush app;
  app.start();
  return 0;
}
