#-------------------------------------------------------------------------------
# Name:        modulo1
# Purpose:
#
# Author:      tasora
#
# Created:     14/02/2012
# Copyright:   (c) tasora 2012
# Licence:     <your licence>
#-------------------------------------------------------------------------------
#!/usr/bin/env python

def main():
    pass

if __name__ == '__main__':
    main()


import os
import math
import ChronoEngine_PYTHON_core as chrono
import ChronoEngine_PYTHON_postprocess as postprocess


# ---------------------------------------------------------------------
#
#  Create the simulation system.
#  (Do not create parts and constraints programmatically here, we will
#  load a mechanism from file)

my_system = chrono.ChSystem()


# Set the collision margins. This is expecially important for very large or
# very small objects (as in this example)! Do this before creating shapes.
chrono.ChCollisionModel.SetDefaultSuggestedEnvelope(0.001);
chrono.ChCollisionModel.SetDefaultSuggestedMargin(0.001);


# ---------------------------------------------------------------------
#
#  load the file generated by the SolidWorks CAD plugin
#  and add it to the ChSystem
#

print ("Loading C::E scene...");

exported_items = chrono.ImportSolidWorksSystem('../../../data/solid_works/swiss_escapement')

print ("...done!");

# Print exported items
for my_item in exported_items:
    print (my_item.GetName())

# Add items to the physical system
for my_item in exported_items:
    my_system.Add(my_item)


# ---------------------------------------------------------------------
#
#  Render a short animation by generating scripts
#  to be used with POV-Ray
#

pov_exporter = postprocess.ChPovRay(my_system)

 # Sets some file names for in-out processes.
pov_exporter.SetTemplateFile        ("../../../data/_template_POV.pov")
pov_exporter.SetOutputScriptFile    ("rendering_frames.pov")
if not os.path.exists("output"):
    os.mkdir("output")
if not os.path.exists("anim"):
    os.mkdir("anim")
pov_exporter.SetOutputDataFilebase("output/my_state")
pov_exporter.SetPictureFilebase("anim/picture")

 # Sets the viewpoint, aimed point, lens angle
pov_exporter.SetCamera(chrono.ChVectorD(0.2,0.3,0.5), chrono.ChVectorD(0,0,0), 35)

 # Sets the default ambient light and default light lamp
pov_exporter.SetAmbientLight(chrono.ChColor(1,1,0.9))
pov_exporter.SetLight(chrono.ChVectorD(-2,2,-1), chrono.ChColor(0.9,0.9,1.1), 1)

 # Sets other settings
pov_exporter.SetPictureSize(640,480)
pov_exporter.SetAmbientLight(chrono.ChColor(2,2,2))

 # If wanted, turn on the rendering of COGs, reference frames, contacts:
#pov_exporter.SetShowCOGs  (1, 0.05)
#pov_exporter.SetShowFrames(1, 0.02)
#pov_exporter.SetShowLinks(1, 0.03)
#pov_exporter.SetShowContacts(1,
#                            postprocess.ChPovRay.SYMBOL_VECTOR_SCALELENGTH,
#                            0.01,   # scale
#                            0.0007, # width
#                            0.1,    # max size
#                            1,0,0.5 ) # colormap on, blue at 0, red at 0.5

 # Add additional POV objects/lights/materials in the following way, entering
 # an optional text using the POV scene description laguage. This will be
 # appended to the generated .pov file.
 # For multi-line strings, use the python ''' easy string delimiter.
pov_exporter.SetCustomPOVcommandsScript(
'''
light_source{ <1,3,1.5> color rgb<0.9,0.9,0.8> }
''')

 # Tell which physical items you want to render
pov_exporter.AddAll()


 # 1) Create the two .pov and .ini files for POV-Ray (this must be done
 #    only once at the beginning of the simulation).
pov_exporter.ExportScript()

 # Configure the solver, if needed
my_system.SetLcpSolverType(chrono.ChSystem.LCP_ITERATIVE_BARZILAIBORWEIN)
my_system.SetIterLCPmaxItersSpeed(40)
my_system.SetMaxPenetrationRecoverySpeed(0.002)
my_system.Set_G_acc(chrono.ChVectorD(0,-9.8,-9.80))

 # Perform a short simulation
nstep =0
while (my_system.GetChTime() < 1.2) :

    my_system.DoStepDynamics(0.002)

    #if math.fmod(nstep,10) ==0 :
    print ('time=', my_system.GetChTime() )

        # 2) Create the incremental nnnn.dat and nnnn.pov files that will be load
        #    by the pov .ini script in POV-Ray (do this at each simulation timestep)
    pov_exporter.ExportData()

    nstep = nstep +1



