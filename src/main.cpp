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
#include "Gamma/Domain.h"

#include "al/scene/al_PolySynth.hpp"
#include "al/scene/al_SynthSequencer.hpp"
#include "al/ui/al_ControlGUI.hpp"
#include "al/ui/al_Parameter.hpp"
#include "al/io/al_Window.hpp"
#include "al/system/al_Time.hpp"
#include "al/ui/al_PresetSequencer.hpp"


#include <vector>
#include "sound.hpp"

using namespace al;


struct RayBrush : App {

  //variables for graphics
  Material material;
  Light light;
  Mesh mMesh;

  std::vector <Vec3f> pos; // keeps position for where to draw each sphere
  std::vector <Color> colorSpheres; //keeps track of color for each drawing stroke
  std::vector <int> start_stroke_positions; // keeps track of where all the stroke positions start for the undo function 
  Color colorPicker{1, 1, 1};
  bool move_with_mouse = false;
  
  
  //for loop pedal
  bool recordLoop = false;
  bool playLoop = false;
  std::vector <int> loopMidiNotes; // keeps track of the notes for each stroke for Loop Pedal
  std::vector <double> loopDurations; // keeps track of the durations for each note in the loop Pedal
  

  //variables for sound
  SynthGUIManager<Sound> synthManager{"Sound"};
  int midiNote = 0;
  float frequency = 0;
  Timer durationTimer;

  void onCreate() override {
    //for graphics
    nav().pos(0, 0, 100); //zoom in and out, higher z is out farther away
    light.pos(0, 0, 100); // where the light is set
    addSphere(mMesh, 0.2); 
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
    //3 settings Loop pedal: record, play, off
    ImGui::RadioButton("Record Loop", recordLoop); //Spacebar is pressed and loop recording is turned on 
    ImGui::RadioButton("Play Loop", playLoop); //Spacebar is pressed while recordLoop is true, recordLoop becomes 
    // false and playLoop becomes true, looped recording is turned on
    


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
    
    midiNote = (m.x()+m.y())%40 + 60; //(range from 60-100)
    std::cout<<"Drawing midi note = "<< midiNote <<std::endl;
    //std::cout<<"currentSphereCount = "<< currentSphereCount <<std::endl;
    const float A4 = 220.f;
    if (midiNote > 0) {
        synthManager.voice()->setInternalParameterValue(
            "frequency", ::pow(2.f, (midiNote - 69.f) / 12.f) * A4);
        synthManager.triggerOn(midiNote);
      }
    Color sphereColor = colorPicker;
    colorSpheres.push_back(sphereColor);
    if(recordLoop == true){
      loopMidiNotes.push_back(midiNote);
    }
    
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
    //trigger note off
    synthManager.triggerOff(midiNote);
    if(recordLoop == true){
      loopDurations.push_back(durationTimer.elapsedSec());
    }

    return true;
  }



  //for Loop Pedal Spacebar
   bool onKeyDown(const Keyboard &k) override {
     if( k.key() == ' ' && recordLoop == false && playLoop == false){ //start recording loop
          //set recordLoop to true
          recordLoop = true;
     }
      else if( k.key() == ' ' && recordLoop == true && playLoop == false){ //start playing recorded loop on repeat with sequencer 
          playLoop = true;
          recordLoop = false;
          double start = 0.0;
          for( int i = 0; i < loopMidiNotes.size(); i++){
            //add notes to sequencer with their corresponding durations
            std::cout<<"adding durations, notes " << start << " " <<loopDurations[i]<<std::endl;
            sequencer().add<Sound>(start, loopDurations[i]).setInternalVals(loopMidiNotes[i]);
            start = start +  loopDurations[i];
          }
          std::cout<<"about to play sequence  " <<std::endl;
          sequencer().playSequence();
          std::cout<<"finished playing sequence  " <<std::endl;
      }else if(k.key() == ' ' && recordLoop == false && playLoop == true){
        playLoop = false;
        //stop playing recording, clear loopMidiNotes and loopDurations
        sequencer().stopSequence();
        //trigger off the recorded sequence 
        loopDurations.clear();
        loopMidiNotes.clear();
        //clear the sequencer of all notes
        sequencer().getSequenceList().clear();
      }
  
    return true;
  }
  // A simple function to return a reference to the sequencer. This looks
  // nicer than just using the internal variable.
  SynthSequencer &sequencer() { return mSequencer; }


  //use sequencncer->synthManager(), seq.synth().getVoice<InstrumentCelesta>() add voice 
  //count duration of mouse sequence and add duration with voice after to make chords
private:
    SynthSequencer mSequencer;
};
int main() {
  RayBrush app;
  app.start();
  return 0;
}
