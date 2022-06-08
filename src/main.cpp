/*
Allocore 

Description:
Uses mouse as a paintbrush to draw 3D spheres that are mapped sonically using 2D coordinates.


*/
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
#include "sound.hpp"
#include "subsound.hpp"
#include "brushstroke.hpp"

using namespace al;


struct MusicBrush : App {

  //variables for graphics
  Material material;
  Light light;
  Mesh mMesh;

  Color colorPicker{1.f, 1.f, 1.f, 1.f};
  bool move_with_mouse = false;
  int sequenceFileNum = 0;
  std::vector<BrushStroke> brushstrokes; 
  
  //for loop pedal
  bool recordLoop = false;
  bool playLoop = false;

  float loopCounter= 0.f;
  Timer time;

  //variables for sound
  SynthGUIManager<Sub> synthManager{"Sub"};
  int midiNote = 0;
  float frequency = 0;

  Clock clock; 
  float recordDuration = 0.f;
  

  void onCreate() override {
    vsync(); //set screen refresh rate to same as monitor

    clock.useRT();
    //for graphics
    nav().pos(0, 0, 100); 
    light.pos(0, 0, 100); // where the light is set

    addSphere(mMesh, 0.2);
    mMesh.generateNormals();
    //mMesh.smooth();

    // disable nav control mouse drag to look
    navControl().useMouse(false);

    
    // Set sampling rate for Gamma objects from app's audio
    gam::sampleRate(audioIO().framesPerSecond());
    imguiInit();
    synthManager.synthRecorder().verbose(true);
    synthManager.synthSequencer().verbose(true);

    synthManager.synthSequencer().setDirectory("Sub-data");
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
    
    // Edit 4 floats representing a color , r g b a
    ImGui::ColorEdit4("Color", colorPicker.components);
    ImGui::Checkbox("Move With Mouse", &move_with_mouse);
    if (move_with_mouse) {
      navControl().useMouse(!isImguiUsingInput()); //allows screen to move with mouse
    }
    //3 settings Loop pedal: record, play, off
    //ImGui::RadioButton("Record Loop", recordLoop); //Spacebar is pressed and loop recording is turned on 
    //ImGui::RadioButton("Play Loop", playLoop); //Spacebar is pressed while recordLoop is true, recordLoop becomes 
    // false and playLoop becomes true, looped recording is turned on
    ImGui::Checkbox("Record Loop", &recordLoop);
    ImGui::Checkbox("Play Loop", &playLoop);

    ImGui::End();
    imguiEndFrame();
  }


  virtual void onDraw(Graphics &g) override { 
    
    g.clear();
    gl::depthTesting(true);
    g.lighting(true);


    //load sequence, for each active voice, use getInternalParameterValue(startPos)
        //for each active voice,  make this the start pos and draw the strokes that start there
    auto *voices = synthManager.synthSequencer().synth().getActiveVoices();
  
  while (voices  ) {
    std::cout<<"voices id = "  << voices<<std::endl;
    float red = voices->getInternalParameterValue("colorR");
    float green = voices->getInternalParameterValue("colorG");
    float blue = voices->getInternalParameterValue("colorB");
    float alpha = voices->getInternalParameterValue("colorA");
    Color colorSphere{red, green,blue,alpha};

    int currentBrushStrokeIndex = voices->getInternalParameterValue("brushStrokeIndex") ;
    BrushStroke currentBrushStroke = brushstrokes[currentBrushStrokeIndex];
    int strokeSize = currentBrushStroke.dots.size();

  // int j = 0;
   float currentLoopStart = voices->getInternalParameterValue("startStrokeTime");
   float duration = 0.f;

    
   
  clock.update();
  //std::cout<<"currentBrushStrokeIndex = "  << currentBrushStrokeIndex <<std::endl;
  //std::cout<<"currentLoopStart = "  << currentLoopStart<<std::endl;
  //std::cout<<"clocknow = "  << clock.now()<<std::endl;
  //std::cout<<"loopcounter = "  << loopCounter<<std::endl;

  time.stop();
  float seconds = time.elapsedSec();
  time.start();
  //std::cout<<"seconds = "  << seconds<<std::endl;

  
  for( int j = 0; j < strokeSize; j++){
   if(( ( currentLoopStart + (loopCounter * seconds) ) + currentBrushStroke.dots[j].second <= clock.now()) ){
    //if( ( (currentLoopStart + ( loopCounter  * recordDuration) ) + currentBrushStroke.dots[j].second <= clock.now()) ){
        //std::cout<<"currentBrushStroke.dots[j].second = "  << currentBrushStroke.dots[j].second <<std::endl;
        // std::cout<<"currentLoopStart = "  << currentLoopStart<<std::endl;
        
        // std::cout<<"clocknow = "  << clock.now()<<std::endl;
          g.pushMatrix();
          g.translate(currentBrushStroke.dots[j].first);
          g.color(colorSphere);
          g.draw(mMesh);
          g.popMatrix();
          clock.update();
          //j ++;
    }
    
  }
    //std::cout<<"voice->next = "  << voices->next<<std::endl;
    voices = voices->next;
    }
    
    //
    // Render the synth's graphics
    synthManager.render(g);
    // GUI is drawn here
    imguiDraw();
    //change z coord with gui
    //std::cout<<"posZ: " << synthManager.voice()->getInternalParameterValue("z") <<std::endl;
    nav().pos(0, 0, synthManager.voice()->getInternalParameterValue("z")); //zoom in and out, higher z is out farther away
    

    if(!synthManager.synthSequencer().playing() && playLoop == true ){ // keeps looping if playLoop is on
        std::string sequenceName = "sound" + std::to_string(sequenceFileNum-1); //loop prev file 
        std::cout<<"play sequence: " << sequenceName <<std::endl;
        synthManager.synthSequencer().playSequence(sequenceName + ".synthSequence"); 
        loopCounter++; 
        recordDuration = synthManager.synthSequencer().getSequenceDuration(sequenceName + ".synthSequence");
        time.start();
        }     
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
    std::cout<<"Mouse Down"<< std::endl;
  
    Vec3d screenPos;
    screenPos.x = (m.x() * 1. / width()) * 2. - 1.;
    screenPos.y = ((height() - m.y()) * 1. / height()) * 2. - 1.;
    screenPos.z = 1.;
    Vec3d worldPos = unproject(screenPos);
    Vec3f position = Vec3f(worldPos.x, worldPos.y, worldPos.z);
    //pos.push_back(position); first sphere is drawn on onProcess

    Color sphereColor = colorPicker;

    
    synthManager.voice()->setInternalParameterValue("posX",worldPos.x);
    synthManager.voice()->setInternalParameterValue("posY",worldPos.y);
    synthManager.voice()->setInternalParameterValue("posZ",worldPos.z);
    synthManager.voice()->setInternalParameterValue("colorR",sphereColor.r);
    synthManager.voice()->setInternalParameterValue("colorG",sphereColor.g);
    synthManager.voice()->setInternalParameterValue("colorB",sphereColor.b);
    synthManager.voice()->setInternalParameterValue("colorA",sphereColor.a);
    synthManager.voice()->setInternalParameterValue("brushStrokeIndex", brushstrokes.size());
    clock.update();
    synthManager.voice()->setInternalParameterValue("startStrokeTime", clock.now());


    // use color to change timbre
    std::cout<<"sphereColor.a = "<< sphereColor.a <<std::endl; 
    float timbre = (sphereColor.a); 
    std::cout<<"Timbre = "<< timbre <<std::endl;   
    updateTimbre(timbre);


    //trigger note on

    
    //mouse origin is upper left corner (0,0) -> (1200,800)
    //note mapping: lower left is lower freq, higher and up to right gives higher freq
    midiNote = (m.x() + (800-m.y()))/50 + 65; //(range from 65-105)


    std::cout<<"Drawing midi note = "<< midiNote <<std::endl;
    const float A4 = 220.f;
    if (midiNote > 0) {
        synthManager.voice()->setInternalParameterValue(
            "frequency", ::pow(2.f, (midiNote - 69.f) / 12.f) * A4);
        synthManager.triggerOn(midiNote);
      }

    //add a new brushstroke to brushstrokes
    BrushStroke newStroke;
    newStroke.color = sphereColor;
    brushstrokes.push_back(newStroke);

    return true;
  }

  void updateTimbre(float timbre){
      synthManager.voice()->setInternalParameterValue("cf1", 
        synthManager.voice()->getInternalParameterValue("cf1") * timbre);
      synthManager.voice()->setInternalParameterValue("cf2", 
        synthManager.voice()->getInternalParameterValue("cf2") * timbre);
      synthManager.voice()->setInternalParameterValue("cf3", 
        synthManager.voice()->getInternalParameterValue("cf4") * timbre);
      synthManager.voice()->setInternalParameterValue("bw1", 
        synthManager.voice()->getInternalParameterValue("bw1") * timbre);
      synthManager.voice()->setInternalParameterValue("bw2", 
        synthManager.voice()->getInternalParameterValue("bw2") * timbre);
  }

  bool onMouseDrag(const Mouse &m) override {
    std::cout<<"Mouse Drag " <<std::endl;
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

    Color sphereColor = colorPicker;
    

    synthManager.voice()->setInternalParameterValue("posX",worldPos.x);
    synthManager.voice()->setInternalParameterValue("posY",worldPos.y);
    synthManager.voice()->setInternalParameterValue("posZ",worldPos.z);
    synthManager.voice()->setInternalParameterValue("colorR",sphereColor.r);
    synthManager.voice()->setInternalParameterValue("colorG",sphereColor.g);
    synthManager.voice()->setInternalParameterValue("colorB",sphereColor.b);
    synthManager.voice()->setInternalParameterValue("colorA",sphereColor.a);
    float currentStart = synthManager.voice()->getInternalParameterValue("startStrokeTime");


    clock.update(); 
    float timeElapsedFromStartStroke = clock.now()- currentStart;

    //add dragged dots to burshstrokes most recent stroke
    brushstrokes.back().dots.push_back(std::pair<Vec3d, al_sec>(position, timeElapsedFromStartStroke));
    

    return true;
  }

  bool onMouseMove(const Mouse &m) override {
    //to show what note you will play
    midiNote = (m.x() + (800-m.y()))/50 + 65; //(range from 65-105)
    std::cout<<"Click to play midi note = "<< midiNote <<std::endl;
    return true;
  }

  bool onMouseUp(const Mouse &m) override {
    if(isImguiUsingInput()){
      return true;
    }

    std::cout<<"Mouse Up Trigger off: "<< midiNote<< std::endl;

    
    //trigger note off
    synthManager.triggerOff(midiNote);
    
    //change timbre back
    Color sphereColor = colorPicker;
    float original_timbre = 1.0/(sphereColor.a); 
    updateTimbre(original_timbre);

    return true;
  }


  //for Loop Pedal Spacebar
   bool onKeyDown(const Keyboard &k) override {
     if( k.key() == ' ' && recordLoop == false ){ //start recording loop
          recordDuration = 0.f;
          //set recordLoop to true
          recordLoop = true; 
          std::string sequenceName = "sound" + std::to_string(sequenceFileNum);
          std::cout<<"start recording in file: sequenceName " << sequenceName <<std::endl;
          synthManager.synthRecorder().startRecord(sequenceName, true);
          
     }
      else if( k.key() == ' ' && recordLoop == true ){ //start playing recorded loop on repeat with sequencer 
          std::cout<<"stop recording " <<std::endl;
          synthManager.synthRecorder().stopRecord();
          const float A4 = 220.f;
          playLoop = true;
          recordLoop = false;
          std::string sequenceName = "sound" + std::to_string(sequenceFileNum);
          std::cout<<"play sequence: " << sequenceName <<std::endl;
          sequenceFileNum++;

          loopCounter = 0.f;   
          
      } else if(k.shift() ){
        std::cout<<"shift pressed, end song " <<std::endl;
        //current sequence should finish and stop looping
        playLoop = false;
      }
  
    return true;
  }

};
int main() {
  MusicBrush app;
  // Set window size
  app.dimensions(1200, 800);
  app.configureAudio(48000., 512, 2, 0);
  app.start();
  return 0;
}
