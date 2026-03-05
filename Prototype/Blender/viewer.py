import numpy as np
import os
import bpy
import time
import random

'''
Description: Metadata object that holds mesh properties
             and handles for later use
'''
class Metadata:        
    def instantiate(self, rgba, mesh, base):
        self.attr = mesh.color_attributes["edge_color"].data
        self.rgba = rgba
        self.mesh = mesh
        self.base = base
                
sim_size = 10 # size of the instantiated mesh cube
sim_metadata = Metadata() # globally scoped mesh metadata
sim_colors = [(0, 0, 0, 0.0), (1.0, 0, 0, 1.0), (0, 1.0, 0, 1.0), (0, 0, 1.0, 1.0), (1.0, 1.0, 0, 1.0), (1.0, 0, 1.0, 1.0), (0, 1.0, 1.0, 1.0)] # colors used for representative edge values
sim_edges = [] # globally scoped edge data

'''
Description: Given an edge type and 1D array location,
             returns a list of indices that consitute the
             "first" possible face connected to this edge
Note: Yes, it is ugly and horribly inefficient. Will refactor/scrap
      later as needed...
'''
def face_1(type, index):
    face = []
    if (type == 0):
        mini_index = index % (sim_size*sim_size*2)
        mega_index = (int(index / (sim_size*sim_size*2)) + 1) * sim_size*sim_size*2
        face = [mega_index + mini_index*2, mega_index + mini_index*2 + 1]
    elif (type == 1):
        mini_index = (index*2 + 1) % (sim_size*sim_size*4)
        mega_index = (int(index / (sim_size*sim_size*2)) + 1) * sim_size*sim_size*2
        mooga_index = (index*2 + sim_size*4) % (sim_size*sim_size*4)
        face = [mega_index + mini_index, mega_index + mooga_index]
    elif (type == 2):
        mini_index = (index*2 - 2) % (sim_size*sim_size*4)
        mega_index = (int(index / (sim_size*sim_size*2)) + 1) * sim_size*sim_size*2
        mooga_index = (index*2) % (sim_size*sim_size*4)
        face = [mega_index + mini_index, mega_index + mooga_index]
    elif (type == 3):
        mini_index = (index*2 + 2) % (sim_size*sim_size*4)
        mega_index = (int(index / (sim_size*sim_size*2)) + 1) * sim_size*sim_size*2
        mooga_index = (index*2) % (sim_size*sim_size*4)
        face = [mega_index + mini_index, mega_index + mooga_index]
    elif (type == 4):
        mini_index = ((index - (sim_size*sim_size*2)) % (sim_size*sim_size*4)) + 1
        mega_index = (int((index - (sim_size*sim_size*2)) / (sim_size*sim_size*4)) * (sim_size*sim_size*4)) + (sim_size*sim_size*2)
        muuga_index = (int((index - (sim_size*sim_size*2)) / (sim_size*sim_size*4)) * (sim_size*sim_size*4))
        mooga_index = (int(((index - (sim_size*sim_size*2)) % (sim_size*sim_size*4)) / 2))
        face = [mega_index + mini_index, muuga_index + mooga_index]
    elif (type == 5):
        mini_index = ((index - (sim_size*sim_size*2)) % (sim_size*sim_size*4)) - 1
        mega_index = (int((index - (sim_size*sim_size*2)) / (sim_size*sim_size*4)) * (sim_size*sim_size*4)) + (sim_size*sim_size*2)
        muuga_index = (int((index - (sim_size*sim_size*2)) / (sim_size*sim_size*4)) * (sim_size*sim_size*4))
        mooga_index = (int(((index - (sim_size*sim_size*2)) % (sim_size*sim_size*4)) / 2))
        face = [mega_index + mini_index, muuga_index + mooga_index]
    elif (type == 6):
        mini_index = ((index - (sim_size*sim_size*2)) % (sim_size*sim_size*4)) - 2
        mega_index = (int((index - (sim_size*sim_size*2)) / (sim_size*sim_size*4)) * (sim_size*sim_size*4)) + (sim_size*sim_size*2)
        muuga_index = (int((index - (sim_size*sim_size*2)) / (sim_size*sim_size*4)) * (sim_size*sim_size*4))
        mooga_index = (int(((index - (sim_size*sim_size*2)) % (sim_size*sim_size*4)) / 2))
        face = [mega_index + mini_index, muuga_index + mooga_index]
    elif (type == 7):
        mini_index = ((index - (sim_size*sim_size*2)) % (sim_size*sim_size*4)) - 2
        mega_index = (int((index - (sim_size*sim_size*2)) / (sim_size*sim_size*4)) * (sim_size*sim_size*4)) + (sim_size*sim_size*2)
        muuga_index = (int((index - (sim_size*sim_size*2)) / (sim_size*sim_size*4)) * (sim_size*sim_size*4))
        mooga_index = (int(((index - (sim_size*sim_size*2) + (sim_size*4)) % (sim_size*sim_size*4)) / 2))
        face = [mega_index + mini_index, muuga_index + mooga_index]
    elif (type == 8):
        mini_index = ((index) % (sim_size*sim_size*4)) - 2
        mega_index = (int((index) / (sim_size*sim_size*4)) * (sim_size*sim_size*4))
        muuga_index = mega_index - sim_size*sim_size*2
        mooga_index = (int(((index) % (sim_size*sim_size*4)) / 2)) - 1
        face = [mega_index + mini_index, muuga_index + mooga_index]
    elif (type == 9):
        mini_index = ((index) % (sim_size*sim_size*4)) - 2
        mega_index = (int((index) / (sim_size*sim_size*4)) * (sim_size*sim_size*4))
        muuga_index = mega_index - sim_size*sim_size*2
        mooga_index = (int(((index) % (sim_size*sim_size*4)) / 2)) - 1
        face = [mega_index + mini_index, muuga_index + mooga_index]
    elif (type == 10):
        mini_index = ((index) % (sim_size*sim_size*4)) - 2
        mega_index = (int((index) / (sim_size*sim_size*4)) * (sim_size*sim_size*4))
        muuga_index = ((int((index) / (sim_size*sim_size*4)) + 1) * (sim_size*sim_size*4))
        mooga_index = (int(((index) % (sim_size*sim_size*4)) / 2)) - 0
        face = [mega_index + mini_index, muuga_index + mooga_index]
    elif (type == 11):
        mini_index = ((index) % (sim_size*sim_size*4)) - 2
        mega_index = (int((index) / (sim_size*sim_size*4)) * (sim_size*sim_size*4))
        muuga_index = ((int((index) / (sim_size*sim_size*4)) + 1) * (sim_size*sim_size*4))
        mooga_index = (int(((index + sim_size*4) % (sim_size*sim_size*4)) / 2)) - 0
        face = [mega_index + mini_index, muuga_index + mooga_index]
    for e in face:
        if e >= len(sim_edges):
            return []
    return face

'''
Description: Given an edge type and 1D array location,
             returns a list of indices that consitute the
             "second" possible face connected to this edge
Note: Yes, it is ugly and horribly inefficient. Will refactor/scrap
      later as needed...
'''
def face_2(type, index):
    face = []
    if (type == 0):
        mini_index = (index - 2) % (sim_size*sim_size*2)
        mega_index = (int(index / (sim_size*sim_size*2)) + 1) * sim_size*sim_size*2
        face = [mega_index + mini_index*2 + 2, mega_index + mini_index*2 + 3]
    elif (type == 1): # VERIFIED
        mini_index = (index*2 + 1) % (sim_size*sim_size*4)
        mega_index = (int(index / (sim_size*sim_size*2)) - 2) * sim_size*sim_size*2
        mooga_index = (index*2 + sim_size*4) % (sim_size*sim_size*4)
        face = [mega_index + mini_index, mega_index + mooga_index]
    elif (type == 2):
        mini_index = (index*2 - 1 - sim_size*4) % (sim_size*sim_size*4)
        mega_index = (int(index / (sim_size*sim_size*2)) + 1) * sim_size*sim_size*2
        mooga_index = (index*2 + 1 - sim_size*4) % (sim_size*sim_size*4)
        face = [mega_index + mini_index, mega_index + mooga_index]
    elif (type == 3):
        mini_index = (index*2 + 1) % (sim_size*sim_size*4)
        mega_index = (int(index / (sim_size*sim_size*2)) + 1) * sim_size*sim_size*2
        mooga_index = (index*2 + 3) % (sim_size*sim_size*4)
        face = [mega_index + mini_index, mega_index + mooga_index]
    elif (type == 4):
        mini_index = ((index - (sim_size*sim_size*2)) % (sim_size*sim_size*4)) + 2
        mega_index = (int((index - (sim_size*sim_size*2)) / (sim_size*sim_size*4)) * (sim_size*sim_size*4)) + (sim_size*sim_size*2)
        muuga_index = (int((index - (sim_size*sim_size*2)) / (sim_size*sim_size*4)) * (sim_size*sim_size*4))
        mooga_index = (int(((index - (sim_size*sim_size*2)) % (sim_size*sim_size*4)) / 2)) + 1
        face = [mega_index + mini_index, muuga_index + mooga_index]
    elif (type == 5):
        mini_index = ((index - (sim_size*sim_size*2)) % (sim_size*sim_size*4)) + 2
        mega_index = (int((index - (sim_size*sim_size*2)) / (sim_size*sim_size*4)) * (sim_size*sim_size*4)) + (sim_size*sim_size*2)
        muuga_index = (int((index - (sim_size*sim_size*2)) / (sim_size*sim_size*4)) * (sim_size*sim_size*4))
        mooga_index = (int(((index - (sim_size*sim_size*2)) % (sim_size*sim_size*4)) / 2)) + (sim_size*2) + 1
        face = [mega_index + mini_index, muuga_index + mooga_index]
    elif (type == 6):
        mini_index = ((index - (sim_size*sim_size*2)) % (sim_size*sim_size*4)) + 1
        mega_index = (int((index - (sim_size*sim_size*2)) / (sim_size*sim_size*4)) * (sim_size*sim_size*4)) + (sim_size*sim_size*2)
        muuga_index = (int((index - (sim_size*sim_size*2)) / (sim_size*sim_size*4)) * (sim_size*sim_size*4))
        mooga_index = (int(((index - (sim_size*sim_size*2) - (sim_size*4)) % (sim_size*sim_size*4)) / 2)) + (sim_size*2) + 1
        face = [mega_index + mini_index, muuga_index + mooga_index]
    elif (type == 7):
        mini_index = ((index - (sim_size*sim_size*2)) % (sim_size*sim_size*4)) - 1
        mega_index = (int((index - (sim_size*sim_size*2)) / (sim_size*sim_size*4)) * (sim_size*sim_size*4)) + (sim_size*sim_size*2)
        muuga_index = (int((index - (sim_size*sim_size*2)) / (sim_size*sim_size*4)) * (sim_size*sim_size*4))
        mooga_index = (int(((index - (sim_size*sim_size*2) - (sim_size*4)) % (sim_size*sim_size*4)) / 2)) + (sim_size*2) + 1
        face = [mega_index + mini_index, muuga_index + mooga_index]
    elif (type == 8):
        mini_index = ((index - sim_size*4) % (sim_size*sim_size*4)) + 1
        mega_index = (int((index) / (sim_size*sim_size*4)) * (sim_size*sim_size*4))
        muuga_index = mega_index - sim_size*sim_size*2
        mooga_index = (int(((index - sim_size*4) % (sim_size*sim_size*4)) / 2))
        face = [mega_index + mini_index, muuga_index + mooga_index]
    elif (type == 9):
        mini_index = ((index + sim_size*4) % (sim_size*sim_size*4)) - 1
        mega_index = (int((index) / (sim_size*sim_size*4)) * (sim_size*sim_size*4))
        muuga_index = mega_index - sim_size*sim_size*2
        mooga_index = (int(((index) % (sim_size*sim_size*4)) / 2))
        face = [mega_index + mini_index, muuga_index + mooga_index]
    elif (type == 10):
        mini_index = ((index) % (sim_size*sim_size*4)) + 1
        mega_index = (int((index) / (sim_size*sim_size*4)) * (sim_size*sim_size*4))
        muuga_index = ((int((index) / (sim_size*sim_size*4)) + 1) * (sim_size*sim_size*4))
        mooga_index = (int(((index) % (sim_size*sim_size*4)) / 2)) + 1
        face = [mega_index + mini_index, muuga_index + mooga_index]
    elif (type == 11):
        mini_index = ((index) % (sim_size*sim_size*4)) - 1
        mega_index = (int((index) / (sim_size*sim_size*4)) * (sim_size*sim_size*4))
        muuga_index = ((int((index) / (sim_size*sim_size*4)) + 1) * (sim_size*sim_size*4))
        mooga_index = (int(((index) % (sim_size*sim_size*4)) / 2)) + 1
        face = [mega_index + mini_index, muuga_index + mooga_index]
    for e in face:
        if e >= len(sim_edges):
            return []
    return face

'''
Description: Given an edge type and 1D array location,
             returns a list of indices that consitute the
             "third" possible face connected to this edge
Note: Yes, it is ugly and horribly inefficient. Will refactor/scrap
      later as needed...
'''
def face_3(type, index):
    face = []
    dir = []
    if (type == 0):
        mini_index = (index - 2) % (sim_size*sim_size*2)
        mega_index = (int(index / (sim_size*sim_size*2)) - 2) * sim_size*sim_size*2
        face = [mega_index + mini_index*2 + 2, mega_index + mini_index*2 + 3]
    elif (type == 1):
        mini_index = (index*2 + 3) % (sim_size*sim_size*4)
        mega_index = (int(index / (sim_size*sim_size*2)) - 2) * sim_size*sim_size*2
        mooga_index = (index*2 + sim_size*4 + 2) % (sim_size*sim_size*4)
        face = [mega_index + mini_index, mega_index + mooga_index]
    elif (type == 2):
        mini_index = (index*2 - 1 - sim_size*4) % (sim_size*sim_size*4)
        mega_index = (int(index / (sim_size*sim_size*2)) - 2) * sim_size*sim_size*2
        mooga_index = (index*2 + 1 - sim_size*4) % (sim_size*sim_size*4)
        face = [mega_index + mini_index, mega_index + mooga_index]
    elif (type == 3):
        mini_index = (index*2 + 2) % (sim_size*sim_size*4)
        mega_index = (int(index / (sim_size*sim_size*2)) - 2) * sim_size*sim_size*2
        mooga_index = (index*2) % (sim_size*sim_size*4)
        face = [mega_index + mini_index, mega_index + mooga_index]
    elif (type == 4):
        mini_index = ((index - (sim_size*sim_size*2)) % (sim_size*sim_size*4)) - 2
        mega_index = (int((index - (sim_size*sim_size*2)) / (sim_size*sim_size*4)) * (sim_size*sim_size*4)) + (sim_size*sim_size*2)
        muuga_index = (int((index - (sim_size*sim_size*2)) / (sim_size*sim_size*4)) * (sim_size*sim_size*4)) + (sim_size*sim_size*2) + (sim_size*sim_size*4)
        mooga_index = (int(((index - (sim_size*sim_size*2)) % (sim_size*sim_size*4)) / 2)) - 1
        face = [mega_index + mini_index, muuga_index + mooga_index]
    elif (type == 5):
        mini_index = ((index - (sim_size*sim_size*2)) % (sim_size*sim_size*4)) - 2
        mega_index = (int((index - (sim_size*sim_size*2)) / (sim_size*sim_size*4)) * (sim_size*sim_size*4)) + (sim_size*sim_size*2)
        muuga_index = (int((index - (sim_size*sim_size*2)) / (sim_size*sim_size*4)) * (sim_size*sim_size*4)) + (sim_size*sim_size*2) + (sim_size*sim_size*4)
        mooga_index = (int(((index - (sim_size*sim_size*2)) % (sim_size*sim_size*4)) / 2)) - 1
        face = [mega_index + mini_index, muuga_index + mooga_index]
    elif (type == 6):
        mini_index = ((index - (sim_size*sim_size*2) - (sim_size*4)) % (sim_size*sim_size*4)) + 1
        mega_index = (int((index - (sim_size*sim_size*2)) / (sim_size*sim_size*4)) * (sim_size*sim_size*4)) + (sim_size*sim_size*2)
        muuga_index = (int((index - (sim_size*sim_size*2)) / (sim_size*sim_size*4)) * (sim_size*sim_size*4)) + (sim_size*sim_size*2) + (sim_size*sim_size*4)
        mooga_index = (int(((index - (sim_size*sim_size*2) - (sim_size*4)) % (sim_size*sim_size*4)) / 2)) - 1
        face = [mega_index + mini_index, muuga_index + mooga_index]
    elif (type == 7):
        mini_index = ((index - (sim_size*sim_size*2) + (sim_size*4)) % (sim_size*sim_size*4)) - 1
        mega_index = (int((index - (sim_size*sim_size*2)) / (sim_size*sim_size*4)) * (sim_size*sim_size*4)) + (sim_size*sim_size*2)
        muuga_index = (int((index - (sim_size*sim_size*2)) / (sim_size*sim_size*4)) * (sim_size*sim_size*4)) + (sim_size*sim_size*2) + (sim_size*sim_size*4)
        mooga_index = (int(((index - (sim_size*sim_size*2)) % (sim_size*sim_size*4)) / 2)) - 1
        face = [mega_index + mini_index, muuga_index + mooga_index]
    elif (type == 8):
        mini_index = ((index) % (sim_size*sim_size*4)) + 1
        mega_index = (int((index) / (sim_size*sim_size*4)) * (sim_size*sim_size*4))
        muuga_index = ((int((index) / (sim_size*sim_size*4)) + 1) * (sim_size*sim_size*4))
        mooga_index = (int(((index) % (sim_size*sim_size*4)) / 2))
        face = [mega_index + mini_index, muuga_index + mooga_index]
    elif (type == 9):
        mini_index = ((index) % (sim_size*sim_size*4)) - 1
        mega_index = (int((index) / (sim_size*sim_size*4)) * (sim_size*sim_size*4))
        muuga_index = ((int((index) / (sim_size*sim_size*4)) + 1) * (sim_size*sim_size*4))
        mooga_index = (int(((index) % (sim_size*sim_size*4)) / 2))
        face = [mega_index + mini_index, muuga_index + mooga_index]
    elif (type == 10):
        mini_index = ((index) % (sim_size*sim_size*4)) + 2
        mega_index = (int((index) / (sim_size*sim_size*4)) * (sim_size*sim_size*4))
        muuga_index = mega_index - sim_size*sim_size*2
        mooga_index = (int(((index) % (sim_size*sim_size*4)) / 2))
        face = [mega_index + mini_index, muuga_index + mooga_index]
    elif (type == 11):
        mini_index = ((index) % (sim_size*sim_size*4)) + 2
        mega_index = (int((index) / (sim_size*sim_size*4)) * (sim_size*sim_size*4))
        muuga_index = mega_index - sim_size*sim_size*2
        mooga_index = (int(((index) % (sim_size*sim_size*4)) / 2))
        face = [mega_index + mini_index, muuga_index + mooga_index]
    for e in face:
        if e >= len(sim_edges):
            return []
    return face

'''
Description: Given an edge type and 1D array location,
             returns a list of indices that consitute the
             "fourth" possible face connected to this edge
Note: Yes, it is ugly and horribly inefficient. Will refactor/scrap
      later as needed...
'''
def face_4(type, index):
    face = []
    if (type == 0):
        mini_index = index % (sim_size*sim_size*2)
        mega_index = (int(index / (sim_size*sim_size*2)) - 2) * sim_size*sim_size*2
        face = [mega_index + mini_index*2 + 0, mega_index + mini_index*2 + 1]
    elif (type == 1):
        mini_index = (index*2 + 3) % (sim_size*sim_size*4)
        mega_index = (int(index / (sim_size*sim_size*2)) + 1) * sim_size*sim_size*2
        mooga_index = (index*2 + sim_size*4 + 2) % (sim_size*sim_size*4)
        face = [mega_index + mini_index, mega_index + mooga_index]
    elif (type == 2):
        mini_index = (index*2 - 2) % (sim_size*sim_size*4)
        mega_index = (int(index / (sim_size*sim_size*2)) - 2) * sim_size*sim_size*2
        mooga_index = (index*2) % (sim_size*sim_size*4)
        face = [mega_index + mini_index, mega_index + mooga_index]
    elif (type == 3):
        mini_index = (index*2 + 1) % (sim_size*sim_size*4)
        mega_index = (int(index / (sim_size*sim_size*2)) - 2) * sim_size*sim_size*2
        mooga_index = (index*2 + 3) % (sim_size*sim_size*4)
        face = [mega_index + mini_index, mega_index + mooga_index]
    elif (type == 4):
        mini_index = ((index - (sim_size*sim_size*2) - sim_size*4) % (sim_size*sim_size*4)) + 1
        mega_index = (int((index - (sim_size*sim_size*2)) / (sim_size*sim_size*4)) * (sim_size*sim_size*4)) + (sim_size*sim_size*2)
        muuga_index = (int((index - (sim_size*sim_size*2)) / (sim_size*sim_size*4)) * (sim_size*sim_size*4)) + (sim_size*sim_size*2) + (sim_size*sim_size*4)
        mooga_index = (int(((index - (sim_size*sim_size*2) - sim_size*4) % (sim_size*sim_size*4)) / 2))
        face = [mega_index + mini_index, muuga_index + mooga_index]
    elif (type == 5):
        mini_index = ((index - (sim_size*sim_size*2) + sim_size*4 - 1) % (sim_size*sim_size*4))
        mega_index = (int((index - (sim_size*sim_size*2)) / (sim_size*sim_size*4)) * (sim_size*sim_size*4)) + (sim_size*sim_size*2)
        muuga_index = (int((index - (sim_size*sim_size*2)) / (sim_size*sim_size*4)) * (sim_size*sim_size*4)) + (sim_size*sim_size*2) + (sim_size*sim_size*4)
        mooga_index = ((int(((index - (sim_size*sim_size*2) - sim_size*4) % (sim_size*sim_size*4)) / 2)) + (sim_size*2)) # fix later: modding negative numbers doesn't work
        face = [mega_index + mini_index, muuga_index + mooga_index]
    elif (type == 6):
        mini_index = ((index - (sim_size*sim_size*2)) % (sim_size*sim_size*4)) + 2
        mega_index = (int((index - (sim_size*sim_size*2)) / (sim_size*sim_size*4)) * (sim_size*sim_size*4)) + (sim_size*sim_size*2)
        muuga_index = (int((index - (sim_size*sim_size*2)) / (sim_size*sim_size*4)) * (sim_size*sim_size*4)) + (sim_size*sim_size*2) + (sim_size*sim_size*4)
        mooga_index = ((int(((index - (sim_size*sim_size*2) - sim_size*4) % (sim_size*sim_size*4)) / 2)) + (sim_size*2)) # fix later: modding negative numbers doesn't work
        face = [mega_index + mini_index, muuga_index + mooga_index]
    elif (type == 7):
        mini_index = ((index - (sim_size*sim_size*2)) % (sim_size*sim_size*4)) + 2
        mega_index = (int((index - (sim_size*sim_size*2)) / (sim_size*sim_size*4)) * (sim_size*sim_size*4)) + (sim_size*sim_size*2)
        muuga_index = (int((index - (sim_size*sim_size*2)) / (sim_size*sim_size*4)) * (sim_size*sim_size*4)) + (sim_size*sim_size*2) + (sim_size*sim_size*4)
        mooga_index = ((int(((index - (sim_size*sim_size*2) - sim_size*4) % (sim_size*sim_size*4)) / 2)) + (sim_size*2)) # fix later: modding negative numbers doesn't work
        face = [mega_index + mini_index, muuga_index + mooga_index]
    elif (type == 8):
        mini_index = ((index) % (sim_size*sim_size*4)) + 2
        mega_index = (int((index) / (sim_size*sim_size*4)) * (sim_size*sim_size*4))
        muuga_index = ((int((index) / (sim_size*sim_size*4)) + 1) * (sim_size*sim_size*4))
        mooga_index = (int(((index) % (sim_size*sim_size*4)) / 2)) + 1
        face = [mega_index + mini_index, muuga_index + mooga_index]
    elif (type == 9):
        mini_index = ((index) % (sim_size*sim_size*4)) + 2
        mega_index = (int((index) / (sim_size*sim_size*4)) * (sim_size*sim_size*4))
        muuga_index = ((int((index) / (sim_size*sim_size*4)) + 1) * (sim_size*sim_size*4))
        mooga_index = (int(((index + sim_size*4) % (sim_size*sim_size*4)) / 2)) + 1
        face = [mega_index + mini_index, muuga_index + mooga_index]
    elif (type == 10):
        mini_index = ((index - sim_size*4) % (sim_size*sim_size*4)) + 1
        mega_index = (int((index) / (sim_size*sim_size*4)) * (sim_size*sim_size*4))
        muuga_index = mega_index - sim_size*sim_size*2
        mooga_index = (int(((index - sim_size*4) % (sim_size*sim_size*4)) / 2)) - 1
        face = [mega_index + mini_index, muuga_index + mooga_index]
    elif (type == 11):
        mini_index = ((index + sim_size*4) % (sim_size*sim_size*4)) - 1
        mega_index = (int((index) / (sim_size*sim_size*4)) * (sim_size*sim_size*4))
        muuga_index = mega_index - sim_size*sim_size*2
        mooga_index = (int(((index) % (sim_size*sim_size*4)) / 2)) - 1
        face = [mega_index + mini_index, muuga_index + mooga_index]
    for e in face:
        if e >= len(sim_edges):
            return []
    return face

'''
Description: An edge that holds flow in the simulation
'''
class SimEdge:
    def __init__(self, type, direction, value = 0):
        self.type = type # [0-11]
        self.value = value
        self.direction = direction
    def color(self):
        if (abs(self.value) >= len(sim_colors)):
            return sim_colors[-1]
        return sim_colors[abs(self.value)]
    def random_face_edges(self, index):
        face_index = int(random.random()*4)
        if (face_index == 0):
            return face_1(self.type, index)
        elif (face_index == 1):
            return face_2(self.type, index)
        elif (face_index == 2):
            return face_3(self.type, index)
        elif (face_index == 3):
            return face_4(self.type, index)
        return [0, 0]
    def fire(self, index):
        while abs(self.value) >= 4:
            flow_unit = -1 if self.value < 0 else 1
            redir_flow = flow_unit * self.direction # get flow value in context of the world, not local me
            self.value -= flow_unit # reduction of my flow value
            face = self.random_face_edges(index)
            for i in range(len(face)):
                sim_edges[face[i]].value += sim_edges[face[i]].direction * redir_flow   

'''
Description: Updates the blender mesh data based on the data calculated by the
             simulation.
'''
def update_mesh():
    for i, e in enumerate(sim_edges):
        sim_metadata.rgba[i*2, :] = e.color()
        sim_metadata.rgba[i*2+1, :] = e.color()
    sim_metadata.attr.foreach_set("color", sim_metadata.rgba.ravel())
    sim_metadata.mesh.update()

'''
Description: Restarts the simulation to a default configuration
'''
def restart_simulation():
    for i, e in enumerate(sim_edges):
        e.value = 0
        if i == sim_size*sim_size*2*(sim_size/2) + sim_size*sim_size*4*(sim_size/2) + sim_size*sim_size*4 + sim_size*2:
            e.value = 10
            face = face_1(e.type, i)
            for f in face:
                sim_edges[f].value = 6
    update_mesh()

'''
Description: Generates the default set of edges based on the simulation size,
             configuring their positions in space.
'''
def generate_default_edges():
    edges = [] # (start index, end index, color)
    for z in range(sim_size*2):
        if (z%2 == 0):
            offset = 0
            if (z%4 != 0):
                offset = 0.5
            for x in range(sim_size):
                for y in range(sim_size*2):
                    if (y%2 == 0):
                        sim_edges.append(SimEdge(0 + (z%4)/2, 1 if y%4 == 0 else -1))
                        edges.append(((x + offset, y/2 + offset, z/2 * 0.7), (x + 1 + offset, y/2 + offset, z/2 * 0.7), 0))
                    else:
                        sim_edges.append(SimEdge(2 + (z%4)/2, 1 if x%2 == 0 else -1,))
                        edges.append(((x + offset, (y - 1)/2 + offset, z/2 * 0.7), (x + offset, (y - 1)/2 + 1 + offset, z/2 * 0.7), 0))
        else:
            for x in range(sim_size):
                for y in range(sim_size):
                    if ((z-1)%4 == 0):
                        sim_edges.append(SimEdge(4, 1))
                        edges.append(((x, y, (z-1)/2 * 0.7), (x + 0.5, y + 0.5, (z+1)/2 * 0.7), 0))
                        sim_edges.append(SimEdge(5, -1))
                        edges.append(((x + 1, y, (z-1)/2 * 0.7), (x + 0.5, y + 0.5, (z+1)/2 * 0.7), 0))
                        sim_edges.append(SimEdge(6, -1))
                        edges.append(((x, y + 1, (z-1)/2 * 0.7), (x + 0.5, y + 0.5, (z+1)/2 * 0.7), 0))
                        sim_edges.append(SimEdge(7, 1))
                        edges.append(((x + 1, y + 1, (z-1)/2 * 0.7), (x + 0.5, y + 0.5, (z+1)/2 * 0.7), 0))
                    else:
                        sim_edges.append(SimEdge(8, -1))
                        edges.append(((x, y, (z+1)/2 * 0.7), (x + 0.5, y + 0.5, (z-1)/2 * 0.7), 0))
                        sim_edges.append(SimEdge(9, 1))
                        edges.append(((x + 1, y, (z+1)/2 * 0.7), (x + 0.5, y + 0.5, (z-1)/2 * 0.7), 0))
                        sim_edges.append(SimEdge(10, 1))
                        edges.append(((x, y + 1, (z+1)/2 * 0.7), (x + 0.5, y + 0.5, (z-1)/2 * 0.7), 0))
                        sim_edges.append(SimEdge(11, -1))
                        edges.append(((x + 1, y + 1, (z+1)/2 * 0.7), (x + 0.5, y + 0.5, (z-1)/2 * 0.7), 0))
    return edges

'''
Description: Converts a list of edges to vertices usable for
             the blender pipeline
Note: You may notice that there is a lot of vertex duplication. This is because
      blender does not support coloring edges or storing data per edge. Thus, to
      statically color an edge uniquely, each edge must be represented by 2 unique
      vertices.
'''
def convert_edges_to_vec(edges):
    verts = []
    einds = []
    cols = []
    for v0, v1, rgb in edges:
        c = sim_colors[rgb]
        verts.append(v0)
        cols.append(c)
        verts.append(v1)
        cols.append(c)
        einds.append((len(verts) - 1, len(verts) - 2))
    return verts, einds, cols
        
'''
Description: Generates the default mesh based on the simulation "size"
'''
def generate_default_mesh():
    if "out" in bpy.data.objects:
        obj = bpy.data.objects["out"]
        if obj.type != 'MESH':
            raise TypeError("The \"out\" object should be a mesh")
        mesh = obj.data
        mesh.clear_geometry()
    else:
        raise Exception("No valid object detected - please create a mesh object \"out\" with the corresponding geometry node")
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    verts, einds, cols = convert_edges_to_vec(generate_default_edges())
    mesh.from_pydata(verts, einds, [])
    if "edge_color" not in mesh.color_attributes:
        mesh.color_attributes.new(
            name="edge_color",
            type='FLOAT_COLOR',
            domain='POINT'
        )
    attr = mesh.color_attributes["edge_color"].data
    rgba = np.ones((len(cols), 4), dtype=np.float32)
    rgba[:, :] = np.array(cols, dtype=np.float32)
    attr.foreach_set("color", rgba.ravel())
    mesh.update()
    return mesh, verts, einds, cols

def flow_fire(scene):
    f = scene.frame_current
    if (f == 1):
        print("Running back to start...")
        restart_simulation()
        update_mesh()
        return
    if (f == 2):
        print("Restarting simulation...")
        restart_simulation()
    for i, e in enumerate(sim_edges):
        e.fire(i)
    update_mesh()

'''
Description: Restarts simulation data and attatches flow firing function
             to the animation handler
'''
def embed_flow_firing(mesh, verts, einds, cols):
    base_colors = np.zeros((len(cols), 3), dtype=np.float32)
    attr = mesh.color_attributes["edge_color"].data
    for i, c in enumerate(attr):
        base_colors[i] = c.color[:3]
    rgba = np.ones((len(cols), 4), dtype=np.float32)
    sim_metadata.instantiate(rgba, mesh, base_colors)
    bpy.app.handlers.frame_change_pre.clear()
    bpy.app.handlers.frame_change_pre.append(flow_fire)
    restart_simulation()

if __name__ == "__main__":
    print("\nGenerating mesh...")
    mesh, verts, einds, cols = generate_default_mesh()
    print("Embedding flow firing...")
    embed_flow_firing(mesh, verts, einds, cols)
    print("\033[32mSuccessfully\033[0m finished configuring flow firing!")
