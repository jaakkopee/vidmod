import pygame
import numpy as np
import moviepy.editor as mpy
import soundfile as sf
import cv2

DEBUG = False

class CircularBuffer:
    #a general circular buffer, for any type of data
    def __init__(self, data):
        self.data = data
        self.index = 0

    def getNext(self):
        #get the next element in the buffer
        element = self.data[self.index]
        self.index += 1
        if self.index >= len(self.data):
            self.index = 0
        return element
    
    def setIndex(self, index):
        #set the index of the buffer
        self.index = index
    
    def getBuffer(self, length):
        #get a buffer of elements
        buffer = []
        for i in range(length):
            buffer.append(self.getNext())
        return buffer
    


class CircularAudioBuffer(CircularBuffer):
    #a circular buffer for audio data
    def __init__(self, audio, sr):
        super().__init__(audio)
        self.sr = sr

    def getBuffer(self, length):
        #get a buffer of audio samples
        buffer = []
        for i in range(length):
            buffer.append(self.getNext())
            if self.index == 0:
                #if we reach the end of the audio, loop back to the beginning
                self.index = 0
        return buffer
    
    def get8ChannelFFT(self, audioBuffer):
        #get the fft of the audio buffer
        audio_fft = np.fft.fft(audioBuffer)
        audio_fft = np.abs(audio_fft)
        #get rid of imaginary part
        audio_fft = audio_fft[:len(audio_fft) // 2]
        #normalize
        audio_fft = audio_fft / np.max(audio_fft)
        # split to 8 bands
        filterbank = np.array([np.linspace(0, 1, len(audio_fft)) for _ in range(8)])
        audio_fft = np.dot(filterbank, audio_fft)
        cut = len(audio_fft) // 8
        return audio_fft[:cut], audio_fft[cut:2*cut], audio_fft[2*cut:3*cut], audio_fft[3*cut:4*cut], audio_fft[4*cut:5*cut], audio_fft[5*cut:6*cut], audio_fft[6*cut:7*cut], audio_fft[7*cut:8*cut]
    
    def getFFT(self, audioBuffer):
        #get the fft of the audio buffer
        audio_fft = np.fft.fft(audioBuffer)
        audio_fft = np.abs(audio_fft)
        #get rid of imaginary part
        audio_fft = audio_fft[:len(audio_fft) // 2]
        #normalize
        audio_fft = audio_fft / np.max(audio_fft)
        # split to 3 bands
        filterbank = np.array([np.linspace(0, 1, len(audio_fft)) for _ in range(3)])
        audio_fft = np.dot(filterbank, audio_fft)
        cut = len(audio_fft) // 3
        return audio_fft[:cut], audio_fft[cut:2*cut], audio_fft[2*cut:3*cut]
    
    def getRMS(self, audioBuffer):
        #get the root mean square of the audio buffer
        return np.sqrt(np.mean(np.square(audioBuffer)))
    
class CircularVideoBuffer(CircularBuffer):
    #a circular buffer for video data
    def __init__(self, video):
        super().__init__(video)
    
    def getFrame(self):
        #get the next frame in the buffer
        return self.getNext()
    

def getFrame(video, frameNumber=0):
    #get a frame from the video
    return video.get_frame(frameNumber / video.fps)
    

def diffuseFrame(frame, diffuse_coeff=0.1, iterations=1):
    print("diffuseFrame()")
    frame = frame.astype(np.float32)
    new_frame = np.array(frame)
    # Pre-compute the offsets for neighbors
    offsets = [(i, j) for i in range(-1, 2) for j in range(-1, 2) if not (i == 0 and j == 0)]
    
    for it in range(iterations):
        print("Iteration ", it)
        for y in range(new_frame.shape[0]):
            for x in range(new_frame.shape[1]):
                diff = np.zeros(3)
                for i, j in offsets:
                    nx, ny = x + i, y + j
                    if 0 <= nx < new_frame.shape[1] and 0 <= ny < new_frame.shape[0]:
                        diff += new_frame[ny, nx] - new_frame[y, x]
                diff *= diffuse_coeff
                # Apply biases here if needed
                new_frame[y, x] = new_frame[y, x] + diff
    
    # Clip values to valid range
    new_frame = np.clip(new_frame, 0, 255)
    return new_frame

class ShadowWorker:
    #a class for the shadow worker algorithm
    #A shadow generator. Augments existing shadows in the image.
    def __init__(self, frame, shadow_coeff):
        self.frame = frame
        self.shadow_coeff = shadow_coeff
        self.shadow = np.zeros_like(frame)

    #find local minima in the frame
    def findLocalMinima(self, x, y):
        minima_r = self.frame[y, x, 0]
        minima_g = self.frame[y, x, 1]
        minima_b = self.frame[y, x, 2]

        for i in range(-1, 2):
            for j in range(-1, 2):
                if i == 0 and j == 0:
                    continue
                if x + i < 0 or x + i >= self.frame.shape[1] or y + j < 0 or y + j >= self.frame.shape[0]:
                    continue
                
                pixel_r = self.frame[y + j, x + i, 0]
                pixel_g = self.frame[y + j, x + i, 1]
                pixel_b = self.frame[y + j, x + i, 2]

                if pixel_r < minima_r:
                    minima_r = pixel_r
                if pixel_g < minima_g:
                    minima_g = pixel_g
                if pixel_b < minima_b:
                    minima_b = pixel_b

        minima = np.array([minima_r, minima_g, minima_b])

        return minima

    #apply the shadow worker algorithm, a cell automaton that augments shadows
    def apply(self):
        print ("ShadowWorker.apply()")

        for y in range(self.frame.shape[0]):
            for x in range(self.frame.shape[1]):
                minima = self.findLocalMinima(x, y)
                shadow = self.shadow[y, x]
                shadow = shadow + (minima - shadow) * self.shadow_coeff
                self.shadow[y, x] = shadow

        #check for overflow
        self.shadow = np.clip(self.shadow, 0, 255).astype(np.uint8)

        return self.shadow
        
def castShadow(frame, shadow_coeff):
    shadow = ShadowWorker(frame, shadow_coeff)
    return shadow.apply()

class AudioReactiveShadow:
    #Audio reactive shadow augmentation
    def __init__(self, frame, audioBuffer, sr, shadow_coeff, video_fps):
        self.frame = frame
        self.video_fps = video_fps
        self.audioBuffer = audioBuffer
        self.sr = sr
        self.shadow_coeff = shadow_coeff
        self.shadow = np.zeros_like(frame)

    def findLocalMinima(self, x, y, neighborhood_size):
        minima_r = self.frame[y, x, 0]
        minima_g = self.frame[y, x, 1]
        minima_b = self.frame[y, x, 2]

        for i in range(-neighborhood_size, neighborhood_size + 1):
            for j in range(-neighborhood_size, neighborhood_size + 1):
                if i == 0 and j == 0:
                    continue
                if x + i < 0 or x + i >= self.frame.shape[1] or y + j < 0 or y + j >= self.frame.shape[0]:
                    continue
                
                pixel_r = self.frame[y + j, x + i, 0]
                pixel_g = self.frame[y + j, x + i, 1]
                pixel_b = self.frame[y + j, x + i, 2]

                if pixel_r < minima_r:
                    minima_r = pixel_r
                if pixel_g < minima_g:
                    minima_g = pixel_g
                if pixel_b < minima_b:
                    minima_b = pixel_b

        minima = np.array([minima_r, minima_g, minima_b])

        return minima

    def apply(self):
        print ("AudioReactiveShadow.apply()")
        audioFramesPerVideoFrame = int(self.sr / self.video_fps)
        audio_buffer = self.audioBuffer.getBuffer(audioFramesPerVideoFrame)
        rms = self.audioBuffer.getRMS(audio_buffer)
        print("RMS: ", rms)
        #the size of the neighborhood is proportional to the rms of the audio
        neighborhood_size = int(rms * 10)
        print("Neighborhood size: ", neighborhood_size)
        for y in range(self.frame.shape[0]):
            for x in range(self.frame.shape[1]):
                minima = self.findLocalMinima(x, y, neighborhood_size)
                shadow = self.shadow[y, x]
                shadow = shadow + (minima - shadow) * self.shadow_coeff
                self.shadow[y, x] = shadow

        self.shadow = np.clip(self.shadow, 0, 255).astype(np.uint8)

        return self.shadow
    

def castAudioReactiveShadow(frame, audioBuffer, sr, shadow_coeff, video_fps):
    audioReactiveShadow = AudioReactiveShadow(frame, audioBuffer, sr, shadow_coeff, video_fps)
    return audioReactiveShadow.apply()

class LightWorker:
    #Colors! Light! Diffusion!
    def __init__(self, frame, light_coeff):
        self.frame = frame
        self.light_coeff = light_coeff
        self.light = np.zeros_like(frame)

    def findLocalMaxima(self, x, y):
        maxima_r = self.frame[y, x, 0]
        maxima_g = self.frame[y, x, 1]
        maxima_b = self.frame[y, x, 2]

        for i in range(-1, 2):
            for j in range(-1, 2):
                if i == 0 and j == 0:
                    continue
                if x + i < 0 or x + i >= self.frame.shape[1] or y + j < 0 or y + j >= self.frame.shape[0]:
                    continue
                
                pixel_r = self.frame[y + j, x + i, 0]
                pixel_g = self.frame[y + j, x + i, 1]
                pixel_b = self.frame[y + j, x + i, 2]

                if pixel_r > maxima_r:
                    maxima_r = pixel_r
                if pixel_g > maxima_g:
                    maxima_g = pixel_g
                if pixel_b > maxima_b:
                    maxima_b = pixel_b

        maxima = np.array([maxima_r, maxima_g, maxima_b])

        return maxima

    def apply(self):
        print ("LightWorker.apply()")
        for y in range(self.frame.shape[0]):
            for x in range(self.frame.shape[1]):
                maxima = self.findLocalMaxima(x, y)
                light = self.light[y, x]
                light = light + (maxima - light) * self.light_coeff
                self.light[y, x] = light

        self.light = np.clip(self.light, 0, 255).astype(np.uint8)

        return self.light

def showLight(frame, light_coeff):
    light = LightWorker(frame, light_coeff)
    return light.apply()

class AudioLightShadowWorker:
    #Audio reactive light and shadow augmentation
    def __init__(self, frame, audioBuffer, sr, shadow_coeff, light_coeff, video_fps):
        self.frame = frame
        self.video_fps = video_fps
        self.audioBuffer = audioBuffer
        self.sr = sr
        self.shadow_coeff = shadow_coeff
        self.light_coeff = light_coeff
        self.shadow = np.zeros_like(frame)
        self.light = np.zeros_like(frame)

    def getNeighborhood(self, x, y, neighborhood_size):
        #neigborhood of a pixel including the pixel itself
        x1 = max(0, x - neighborhood_size)
        x2 = min(self.frame.shape[1], x + neighborhood_size + 1)
        y1 = max(0, y - neighborhood_size)
        y2 = min(self.frame.shape[0], y + neighborhood_size + 1)
        return self.frame[y1:y2, x1:x2]

    def findLocalMinima(self, x, y, neighborhood_size):

        neighbors = self.getNeighborhood(x, y, neighborhood_size)

        red = neighbors[:, :, 0]
        green = neighbors[:, :, 1]
        blue = neighbors[:, :, 2]

        minR = np.min(red)
        minG = np.min(green)
        minB = np.min(blue)

        minima = np.array([minR, minG, minB])

        return minima
    
    def findLocalMaxima(self, x, y, neighborhood_size):
            
        neighbors = self.getNeighborhood(x, y, neighborhood_size)

        red = neighbors[:, :, 0]
        green = neighbors[:, :, 1]
        blue = neighbors[:, :, 2]

        maxR = np.max(red)
        maxG = np.max(green)
        maxB = np.max(blue)

        maxima = np.array([maxR, maxG, maxB])

        return maxima
    
    def apply(self):
        print ("AudioLightShadowWorker.apply()")
        audioFramesPerVideoFrame = int(self.sr / self.video_fps)
        audio_buffer = self.audioBuffer.getBuffer(audioFramesPerVideoFrame)
        rms = self.audioBuffer.getRMS(audio_buffer)
        print("RMS: ", rms)
        neighborhood_size = int(rms * 10)
        print("Neighborhood size: ", neighborhood_size)
        print("Cells in neighborhood: ", (neighborhood_size * 2 + 1) ** 2)
        print ("Shadow coefficient: ", self.shadow_coeff * rms)
        print ("Light coefficient: ", self.light_coeff * rms)

        print ("Applying shadow...")
        for y in range(self.frame.shape[0]):
            for x in range(self.frame.shape[1]):
                minima = self.findLocalMinima(x, y, neighborhood_size)
                shadow = self.shadow[y, x]
                shadow = shadow + (minima - shadow) * self.shadow_coeff * rms
                self.shadow[y, x] = shadow

        print ("Applying light...")
        for y in range(self.frame.shape[0]):
            for x in range(self.frame.shape[1]):
                maxima = self.findLocalMaxima(x, y, neighborhood_size)
                light = self.light[y, x]
                light = light + (maxima - light) * self.light_coeff * rms
                self.light[y, x] = light

        self.shadow = np.clip(self.shadow, 0, 255)
        self.light = np.clip(self.light, 0, 255)

        return self.shadow, self.light
    
def castAudioLightShadow(frame, audioBuffer, sr, shadow_coeff, light_coeff, video_fps, lightR, lightG, lightB, shadowR, shadowG, shadowB):
    audioLightShadow = AudioLightShadowWorker(frame, audioBuffer, sr, shadow_coeff, light_coeff, video_fps)
    shadow, light = audioLightShadow.apply()
    r = np.clip(frame[:, :, 0] + light[:, :, 0]*lightR - shadow[:, :, 0]*shadowR, 0, 255)
    g = np.clip(frame[:, :, 1] + light[:, :, 1]*lightG - shadow[:, :, 1]*shadowG, 0, 255)
    b = np.clip(frame[:, :, 2] + light[:, :, 2]*lightB - shadow[:, :, 2]*shadowB, 0, 255)
    return np.dstack((r, g, b))

class FrameFeedback:
    #A class for the frame feedback algorithm
    def __init__(self, frame, old_frame, feedback_coeff, audioBuffer, sr, video_fps):
        self.frame = frame
        self.old_frame = old_frame
        self.feedback_coeff = feedback_coeff
        self.feedback = np.zeros_like(frame)
        self.audioBuffer = audioBuffer
        self.sr = sr
        self.video_fps = video_fps

    def apply(self):
        #getAudioBuffer
        audioFramesPerVideoFrame = int(self.sr / self.video_fps)

        audio_buffer = self.audioBuffer.getBuffer(audioFramesPerVideoFrame)
        rms = self.audioBuffer.getRMS(audio_buffer)
        print("RMS: ", rms)
        #feedback_coeff is proportional to the rms of the audio
        self.feedback_coeff = rms * self.feedback_coeff
        print("Feedback coefficient: ", self.feedback_coeff)
        #modulate the frame with the feedback from old_frame
        print ("FrameFeedback.apply()")
        for y in range(self.frame.shape[0]):
            for x in range(self.frame.shape[1]):
                #get old rgb values
                old_r = self.old_frame[y, x, 0]
                old_g = self.old_frame[y, x, 1]
                old_b = self.old_frame[y, x, 2]
                #get new rgb values
                r = self.frame[y, x, 0]
                g = self.frame[y, x, 1]
                b = self.frame[y, x, 2]
                #calculate feedback
                new_r = (r + old_r * self.feedback_coeff) / 2
                new_g = (g + old_g * self.feedback_coeff) / 2
                new_b = (b + old_b * self.feedback_coeff) / 2
                #set feedback
                self.feedback[y, x, 0] = new_r
                self.feedback[y, x, 1] = new_g
                self.feedback[y, x, 2] = new_b

        self.feedback = np.clip(self.feedback, 0, 255).astype(np.uint8)

        return self.feedback
    
class AudioColorLightShadow:
    #a faster version of the audio reactive light and shadow algorithm, also implements the original audio reactive color algorithm

    def __init__(self, frame, audioBuffer, sr, video_fps, color_coeff, shadow_coeff, light_coeff, lightR, lightG, lightB, shadowR, shadowG, shadowB, nolightshadow, fft_r_coeff, fft_g_coeff, fft_b_coeff):
        #frame, audioBuffer, sr, video_fps, color_coeff, shadow_coeff, light_coeff, lightR, lightG, lightB, shadowR, shadowG, shadowB
        self.frame = frame
        self.audioBuffer = audioBuffer
        self.sr = sr
        self.video_fps = video_fps
        self.shadow_coeff = shadow_coeff
        self.light_coeff = light_coeff
        self.lightR = lightR
        self.lightG = lightG
        self.lightB = lightB
        self.shadowR = shadowR
        self.shadowG = shadowG
        self.shadowB = shadowB
        self.color_coeff = color_coeff
        self.nolightshadow = nolightshadow
        self.fft_r_coeff = fft_r_coeff
        self.fft_g_coeff = fft_g_coeff
        self.fft_b_coeff = fft_b_coeff


    def findLocalMinima(self, x, y, frame):
        minima_r = frame[y, x, 0]
        minima_g = frame[y, x, 1]
        minima_b = frame[y, x, 2]

        for i in range(-1, 2):
            for j in range(-1, 2):
                if i == 0 and j == 0:
                    continue
                if x + i < 0 or x + i >= frame.shape[1] or y + j < 0 or y + j >= frame.shape[0]:
                    continue
                
                pixel_r = frame[y + j, x + i, 0]
                pixel_g = frame[y + j, x + i, 1]
                pixel_b = frame[y + j, x + i, 2]

                if pixel_r < minima_r:
                    minima_r = pixel_r
                if pixel_g < minima_g:
                    minima_g = pixel_g
                if pixel_b < minima_b:
                    minima_b = pixel_b

        minima = np.array([minima_r, minima_g, minima_b])

        return minima
    
    def findLocalMaxima(self, x, y, frame):
        maxima_r = frame[y, x, 0]
        maxima_g = frame[y, x, 1]
        maxima_b = frame[y, x, 2]

        for i in range(-1, 2):
            for j in range(-1, 2):
                if i == 0 and j == 0:
                    continue
                if x + i < 0 or x + i >= frame.shape[1] or y + j < 0 or y + j >= frame.shape[0]:
                    continue
                
                pixel_r = frame[y + j, x + i, 0]
                pixel_g = frame[y + j, x + i, 1]
                pixel_b = frame[y + j, x + i, 2]

                if pixel_r > maxima_r:
                    maxima_r = pixel_r
                if pixel_g > maxima_g:
                    maxima_g = pixel_g
                if pixel_b > maxima_b:
                    maxima_b = pixel_b

        maxima = np.array([maxima_r, maxima_g, maxima_b])

        return maxima
    
    def apply(self):
        print ("AudioColorLightShadow.apply()")
        print ("Applying FFT...")
        audioFramesPerVideoFrame = int(self.audioBuffer.sr / self.video_fps)
        #get the fft of the audio
        audio_buffer = self.audioBuffer.getBuffer(audioFramesPerVideoFrame)
        #apply hanning window
        audio_buffer = audio_buffer * np.hanning(len(audio_buffer))

        #zero pad the audio buffer to the length of the frame(y)
        audio_buffer = np.pad(audio_buffer, (0, self.frame.shape[0]), 'constant')
        audio_fft = np.fft.fft(audio_buffer)
        audio_fft = np.abs(audio_fft)
        #get rid of imaginary part
        audio_fft = audio_fft[:len(audio_fft) // 2]
        #pad again to the length of the frame(y)
        if len(audio_fft) < self.frame.shape[0]:
            audio_fft = np.pad(audio_fft, (0, self.frame.shape[0] - len(audio_fft)), 'constant')
        elif len(audio_fft) > self.frame.shape[0]:
            audio_fft = audio_fft[:self.frame.shape[0]]
        #get the fft of the frame
        frame_red = self.frame[:, :, 0]
        frame_green = self.frame[:, :, 1]
        frame_blue = self.frame[:, :, 2]

        frame_fft_r = np.fft.fft2(frame_red)
        frame_fft_g = np.fft.fft2(frame_green)
        frame_fft_b = np.fft.fft2(frame_blue)

        #apply the fft of the audio to the fft of the frame
        for y in range(self.frame.shape[0]):
            for x in range(self.frame.shape[1]):
                frame_fft_r[y, x] = frame_fft_r[y, x] * audio_fft[y] * self.fft_r_coeff
                frame_fft_g[y, x] = frame_fft_g[y, x] * audio_fft[y] * self.fft_g_coeff
                frame_fft_b[y, x] = frame_fft_b[y, x] * audio_fft[y] * self.fft_b_coeff

        #get the inverse fft
        new_frame = np.zeros_like(self.frame)
        new_frame[:, :, 0] = np.abs(np.fft.ifft2(frame_fft_r))
        new_frame[:, :, 1] = np.abs(np.fft.ifft2(frame_fft_g))
        new_frame[:, :, 2] = np.abs(np.fft.ifft2(frame_fft_b))


        if not self.nolightshadow:
            print ("Applying shadow...")
            for y in range(new_frame.shape[0]):
                for x in range(new_frame.shape[1]):
                    minima = self.findLocalMinima(x, y, new_frame)
                    shadow = new_frame[y, x]
                    shadow = shadow + (minima - shadow) * self.shadow_coeff
                    new_frame[y, x] = shadow

            print ("Applying light...")
            for y in range(self.frame.shape[0]):
                for x in range(self.frame.shape[1]):
                    maxima = self.findLocalMaxima(x, y, new_frame)
                    light = new_frame[y, x]
                    light = light + (maxima - light) * self.light_coeff
                    new_frame[y, x] = light

        new_frame = np.clip(new_frame, 0, 255)

        return new_frame


def applyFFT(audioBuffer, video_fps, frame, new_frame, fft_r_coeff, fft_g_coeff, fft_b_coeff, red_bias, green_bias, blue_bias):
    print ("applyFFT()")
    audioFramesPerVideoFrame = int(audioBuffer.sr / video_fps)
    #get the fft of the audio
    audio_buffer = audioBuffer.getBuffer(audioFramesPerVideoFrame)
    #apply hanning window
    audio_buffer = audio_buffer * np.hanning(len(audio_buffer))

    #zero pad the audio buffer to the length of the frame(y)
    audio_buffer = np.pad(audio_buffer, (0, frame.shape[0]), 'constant')

    audio_fft_r, audio_fft_g, audio_fft_b = audioBuffer.getFFT(audio_buffer)
    #get the fft of the frame
    frame_red = frame[:, :, 0]
    frame_green = frame[:, :, 1]
    frame_blue = frame[:, :, 2]

    frame_fft_r = np.fft.fft2(frame_red)
    frame_fft_g = np.fft.fft2(frame_green)
    frame_fft_b = np.fft.fft2(frame_blue)
    #apply the fft of the audio to the fft of the frame
    frame_fft_r = frame_fft_r * audio_fft_r * fft_r_coeff
    frame_fft_g = frame_fft_g * audio_fft_g * fft_g_coeff
    frame_fft_b = frame_fft_b * audio_fft_b * fft_b_coeff
    #get the inverse fft
    new_frame[:, :, 0] = np.abs(np.fft.ifft2(frame_fft_r))
    new_frame[:, :, 1] = np.abs(np.fft.ifft2(frame_fft_g))
    new_frame[:, :, 2] = np.abs(np.fft.ifft2(frame_fft_b))

    new_frame[:, :, 0] += red_bias
    new_frame[:, :, 1] += green_bias
    new_frame[:, :, 2] += blue_bias

    return new_frame

class AudioColor:
    #this class changes the color of the frame based on the audio, bass, mid, treble = r, g, b
    def __init__(self, frame, audioBuffer, sr, video_fps, color_coeff):
        self.frame = frame
        self.audioBuffer = audioBuffer
        self.sr = sr
        self.video_fps = video_fps
        self.color_coeff = color_coeff

    def apply(self):
        print ("AudioColor.apply()")
        audioFramesPerVideoFrame = int(self.sr / self.video_fps)
        audio_buffer = self.audioBuffer.getBuffer(audioFramesPerVideoFrame)
        #set index back
        self.audioBuffer.setIndex(self.audioBuffer.index - int(self.sr / self.video_fps))
        rms = self.audioBuffer.getRMS(audio_buffer)
        #set index back
        self.audioBuffer.setIndex(self.audioBuffer.index - int(self.sr / self.video_fps))
        print("RMS: ", rms)
        #the audio reactive color is proportional to the rms of the audio
        colorc = rms * self.color_coeff
        print("Color coefficient: ", colorc)
        #get the fft of the audio
        audio_buffer = np.pad(audio_buffer, (0, self.frame.shape[0]), 'constant')
        audio_fft_r, audio_fft_g, audio_fft_b = self.audioBuffer.getFFT(audio_buffer)
        #get the rgb values of the frame
        frame_red = self.frame[:, :, 0]
        frame_green = self.frame[:, :, 1]
        frame_blue = self.frame[:, :, 2]
        #apply the fft of the audio to the rgb values of the frame
        frame_red = frame_red * np.mean(audio_fft_r) * colorc
        frame_green = frame_green * np.mean(audio_fft_g) * colorc
        frame_blue = frame_blue * np.mean(audio_fft_b) * colorc
        new_frame = np.dstack((frame_red, frame_green, frame_blue))
        return new_frame

class AudioColor8:
    #8 channel audio color, colors are mapped to 8 bands of the audio
    def __init__(self, frame, audioBuffer, sr, video_fps, color_coeff, grayscale):
        self.frame = frame
        self.audioBuffer = audioBuffer
        self.sr = sr
        self.video_fps = video_fps
        self.color_coeff = color_coeff
        self.grayscale = grayscale

    def apply(self):
        print ("AudioColor8.apply()")
        #convert frame to grayscale
        if self.grayscale:
            for y in range(self.frame.shape[0]):
                for x in range(self.frame.shape[1]):
                    gray = np.mean(self.frame[y, x])
                    self.frame[y, x] = gray

        audioFramesPerVideoFrame = int(self.sr / self.video_fps)
        audio_buffer = self.audioBuffer.getBuffer(audioFramesPerVideoFrame)
        rms = self.audioBuffer.getRMS(audio_buffer)
        print("RMS: ", rms)
        #the audio reactive color is proportional to the rms of the audio
        colorc = rms * self.color_coeff
        print("Color coefficient: ", colorc)
        #get the fft of the audio
        audio_buffer = np.pad(audio_buffer, (0, self.frame.shape[0]), 'constant')
        audio_fft = self.audioBuffer.get8ChannelFFT(audio_buffer)
        #init colors
        greyOne = np.array([1, 1, 1])
        greys = np.array([greyOne for _ in range(8)])
        for i in range(8):
            greys[i] = greys[i] * audio_fft[i] * colorc + i / 8
        #get the rgb values of the frame
        frame_red = self.frame[:, :, 0]
        frame_green = self.frame[:, :, 1]
        frame_blue = self.frame[:, :, 2]
        #apply the fft of the audio to the rgb values of the frame
        for i in range(8):
            frame_red = frame_red * greys[i][0]
            frame_green = frame_green * greys[i][1]
            frame_blue = frame_blue * greys[i][2]
        new_frame = np.dstack((frame_red, frame_green, frame_blue))
        return new_frame
    
class StaticAudioColor8:
    #8 channel audio color, colors are mapped to 8 bands of the audio
    def __init__(self, frame, audioBuffer, sr, video_fps, color_coeff, grayscale):
        self.frame = frame
        self.audioBuffer = audioBuffer
        self.sr = sr
        self.video_fps = video_fps
        self.color_coeff = color_coeff
        self.grayscale = grayscale

    def apply(self):
        print ("StaticAudioColor8.apply()")
        #convert frame to grayscale
        if self.grayscale:
            for y in range(self.frame.shape[0]):
                for x in range(self.frame.shape[1]):
                    gray = np.mean(self.frame[y, x])
                    self.frame[y, x] = gray

        audioFramesPerVideoFrame = int(self.sr / self.video_fps)
        audio_buffer = self.audioBuffer.getBuffer(audioFramesPerVideoFrame)
        colorc = self.color_coeff
        print("Color coefficient: ", colorc)
        #get the fft of the audio
        audio_buffer = np.pad(audio_buffer, (0, self.frame.shape[0]), 'constant')
        audio_fft = self.audioBuffer.get8ChannelFFT(audio_buffer)
        #init colors
        greyOne = np.array([1, 1, 1])
        greys = np.array([greyOne for _ in range(8)])
        for i in range(8):
            greys[i] = greys[i] * audio_fft[i] * colorc + i / 8
        #get the rgb values of the frame
        frame_red = self.frame[:, :, 0]
        frame_green = self.frame[:, :, 1]
        frame_blue = self.frame[:, :, 2]
        #apply the fft of the audio to the rgb values of the frame
        for i in range(8):
            frame_red = frame_red * greys[i][0]
            frame_green = frame_green * greys[i][1]
            frame_blue = frame_blue * greys[i][2]
        new_frame = np.dstack((frame_red, frame_green, frame_blue))
        return new_frame

class StaticAudioColor8_2:
    #8 channel audio color, colors are mapped to 8 bands of the audio
    def __init__(self, frame, audioBuffer, sr, video_fps, color_coeff, grayscale):
        self.frame = frame
        self.audioBuffer = audioBuffer
        self.sr = sr
        self.video_fps = video_fps
        self.color_coeff = color_coeff
        self.grayscale = grayscale

    def apply(self):
        print ("StaticAudioColor8.apply()")
        #convert frame to grayscale
        if self.grayscale:
            for y in range(self.frame.shape[0]):
                for x in range(self.frame.shape[1]):
                    gray = np.mean(self.frame[y, x])
                    self.frame[y, x] = gray

        audioFramesPerVideoFrame = int(self.sr / self.video_fps)
        audio_buffer = self.audioBuffer.getBuffer(audioFramesPerVideoFrame)
        colorc = self.color_coeff
        print("Color coefficient: ", colorc)
        #get the fft of the audio
        audio_buffer = np.pad(audio_buffer, (0, self.frame.shape[0]), 'constant')
        audio_fft = self.audioBuffer.get8ChannelFFT(audio_buffer)
        #init colors
        greyOne = np.array([1, 1, 1])
        greys = np.array([greyOne for _ in range(8)])
        for i in range(8):
        #    greys[i] = greys[i] * audio_fft[i] * colorc + i / 8
            greys[i] = (greys[i] * audio_fft[i] * colorc + i) / 8
        #get the rgb values of the frame
        frame_red = self.frame[:, :, 0]
        frame_green = self.frame[:, :, 1]
        frame_blue = self.frame[:, :, 2]
        #apply the fft of the audio to the rgb values of the frame
        for i in range(8):
            frame_red = frame_red * greys[i][0]
            frame_green = frame_green * greys[i][1]
            frame_blue = frame_blue * greys[i][2]
        new_frame = np.dstack((frame_red, frame_green, frame_blue))
        return new_frame


def main():
    print ("fftvidmod2.py main()")
    import argparse

    parser = argparse.ArgumentParser(
                    prog='python fftvidmod2.py',
                    description='Modulates video with audio using FFT and differential color diffusion.',
                    epilog='Is what it is.')

    parser.add_argument('videoFile', type=str, help='Video file to modulate')
    parser.add_argument('audioFile', type=str, help='Audio file to modulate with')
    parser.add_argument('outputVideoFile', type=str, help='Output video file')
    parser.add_argument('--algorithm', type=str, default='fft', choices=['diffuse_fft', 'fft_diffuse', 'fft', 'diffuse', 'diffuse_aulisha', 'shadow', "fft_shadow", "shadow_fft", "audsha", "audsha_shadow", "light", "light_shadow", "audsha_light", "aulisha", "fft_aulisha", "fft_shadow", "none", "framefb", "framefb_shadow", "audiocolor", "aulisha_audiocolor", "audiocolor_aulisha", "audiocolor_shadow", "audiocolor_diffuse", "audcollisha", "audiocolor8", "static_audiocolor8", "static_audiocolor8_2"], help='Algorithm to use. Default is fft.')
    parser.add_argument('--diffc', type=float, default=0.0, help='Diffuse the colors with the given coefficient. Default is 0.0 which means no diffusion.')
    parser.add_argument('--fft_r', type=float, default=1.0, help='Coefficient for the red channel FFT. Default is 1.0.')
    parser.add_argument('--fft_g', type=float, default=1.0, help='Coefficient for the green channel FFT. Default is 1.0.')
    parser.add_argument('--fft_b', type=float, default=1.0, help='Coefficient for the blue channel FFT. Default is 1.0.')
    parser.add_argument('--red', type=int, default=64, help='Bias for the red channel. Default is 64.')
    parser.add_argument('--green', type=int, default=64, help='Bias for the green channel. Default is 64.')
    parser.add_argument('--blue', type=int, default=64, help='Bias for the blue channel. Default is 64.')
    parser.add_argument('--shadow', type=float, default=0.1, help='Shadow coefficient for the shadow worker and audsha algorithms. Default is 0.1.')
    parser.add_argument('--light', type=float, default=0.1, help='Light coefficient for the light algorithm. Default is 0.1.')
    parser.add_argument('--lightR', type=float, default=0.1, help='Light coefficient for the red channel for audshalight algorithm. Default is 0.1.')
    parser.add_argument('--lightG', type=float, default=0.1, help='Light coefficient for the green channel for audshalight algorithm. Default is 0.1.')
    parser.add_argument('--lightB', type=float, default=0.1, help='Light coefficient for the blue channel for audshalight algorithm. Default is 0.1.')
    parser.add_argument('--shadowR', type=float, default=0.1, help='Shadow coefficient for the red channel for audshalight algorithm. Default is 0.1.')
    parser.add_argument('--shadowG', type=float, default=0.1, help='Shadow coefficient for the green channel for audshalight algorithm. Default is 0.1.')
    parser.add_argument('--shadowB', type=float, default=0.1, help='Shadow coefficient for the blue channel for audshalight algorithm. Default is 0.1.')
    parser.add_argument('--feedback', type=float, default=0.1, help='Feedback coefficient for the frame feedback algorithm. Default is 0.1.')
    parser.add_argument('--iterations', type=int, default=1, help='Number of iterations for the diffusion algorithm. Default is 1.')
    parser.add_argument('--colorc', type=float, default=1.0, help='Color coefficient for the audiocolor algorithm. Default is 1.0.')
    parser.add_argument('--grayscale', action='store_true', help='Convert the frame to grayscale before applying the audiocolor8 algorithm. Default is False.')
    parser.add_argument('--nolightshadow', action='store_true', help='Use only the fft part of audcollisha algorithm. Default is False.')
    #get value of argument --diffuse from the command line
    # the value is a float
    # if the argument is not present, the default value is True for global variable DIFFUSE
    # if the argument is present, the value is the float value of the argument

    #parse the arguments 
    args = parser.parse_args()

    algorithm = args.algorithm
    diffusion_coeff = args.diffc
    videoFile = args.videoFile
    audioFile = args.audioFile
    outputVideoFile = args.outputVideoFile
    fft_r_coeff = args.fft_r
    fft_g_coeff = args.fft_g
    fft_b_coeff = args.fft_b
    red_bias = args.red
    green_bias = args.green
    blue_bias = args.blue
    shadow_coeff = args.shadow
    light_coeff = args.light
    lightR = args.lightR
    lightG = args.lightG
    lightB = args.lightB
    shadowR = args.shadowR
    shadowG = args.shadowG
    shadowB = args.shadowB
    feedback_coeff = args.feedback
    iterations = args.iterations
    color_coeff = args.colorc
    grayscale = args.grayscale
    nolightshadow = args.nolightshadow


    if audioFile == "":
        print ("No audio file given, using the video audio.")
        audioFile = videoFile
    #initialize pygame
    pygame.init()

    #load the video
    video = mpy.VideoFileClip(videoFile)
    #get the video fps
    video_fps = video.fps
    height = video.h
    width = video.w

    print ("Video FPS: ", video_fps)
    print ("Video height: ", height)
    print ("Video width: ", width)

    #get the frames of the video
    print ("Getting video frames...")
    video_frames = np.array(list(video.iter_frames(fps=video_fps)))
    #swap axes of each frame
    video_frames = np.swapaxes(video_frames, 1, 2)
    #video_frames = np.array([cv2.resize(frame, (width, height)) for frame in video_frames])
    #create a pygame window
    screen = pygame.display.set_mode((width, height))

    #create a circular video buffer
    videoBuffer = CircularVideoBuffer(video_frames)
    #load the audio
    audio, sr = sf.read(audioFile)
    #if stereo, convert to mono
    if len(audio.shape) > 1:
        audio = audio[:, 0]

    print ("Getting audio frames...")
    #create a circular audio buffer
    audioBuffer = CircularAudioBuffer(audio, sr)


    #get the number of frames in the generated video, the playing time is the same as the audio
    num_frames = int(len(audio) / sr * video_fps)
    
    #create a video writer for the output video
    video_output = cv2.VideoWriter(outputVideoFile, cv2.VideoWriter_fourcc(*"mp4v"), video_fps, (width, height))

    print ("Starting the video modulation...")
    #loop through the frames
    for i in range(num_frames):
        #get the next frame
        frame = videoBuffer.getFrame()
        print()
        print ("Frame ", i, " of ", num_frames, ".", sep="")       

        if algorithm == 'diffuse_fft':
            print("Algorithm: diffuse_fft")
            print("Parameters: diffusion_coeff: ", diffusion_coeff, " fft_r_coeff: ", fft_r_coeff, " fft_g_coeff: ", fft_g_coeff, " fft_b_coeff: ", fft_b_coeff, " red_bias: ", red_bias, " green_bias: ", green_bias, " blue_bias: ", blue_bias, " iterations: ", iterations)
            #convert the frame to float
            frame = frame.astype(np.float32)
            #create a new frame
            new_frame = np.zeros_like(frame)
            #diffuse the frame
            audioRMS = audioBuffer.getRMS(audioBuffer.getBuffer(int(sr / video_fps)))
            audioBuffer.setIndex(audioBuffer.index - int(sr / video_fps)) #set index back, because fft needs to get the same audio buffer
            dc = audioRMS * diffusion_coeff
            print ("Diffusion coefficient: ", dc)
            print ("Audio RMS: ", audioRMS)
            new_frame = diffuseFrame(frame, dc, iterations)
            #to float again, also copy the diffused frame to the original frame
            frame = new_frame.astype(np.float32)
            #apply the fft to the frame
            new_frame = applyFFT(audioBuffer, video_fps, frame, new_frame, fft_r_coeff, fft_g_coeff, fft_b_coeff, red_bias, green_bias, blue_bias)

        elif algorithm == 'fft_diffuse':
            #convert the frame to float
            frame = frame.astype(np.float32)
            #create a new frame
            new_frame = np.zeros_like(frame)
            #apply the fft to the frame
            new_frame = applyFFT(audioBuffer, video_fps, frame, new_frame, fft_r_coeff, fft_g_coeff, fft_b_coeff, red_bias, green_bias, blue_bias)
            #to float again, also copy the fft'd frame to the original frame
            frame = new_frame.astype(np.float32)
            #diffuse the frame
            audioRMS = audioBuffer.getRMS(audioBuffer.getBuffer(int(sr / video_fps)))
            audioBuffer.setIndex(audioBuffer.index - int(sr / video_fps))
            dc = audioRMS * diffusion_coeff
            print ("Diffusion coefficient: ", dc)
            print ("Audio RMS: ", audioRMS)
            new_frame = diffuseFrame(frame, dc, iterations)

        elif algorithm == 'fft':
            #convert the frame to float
            frame = frame.astype(np.float32)
            #create a new frame
            new_frame = np.zeros_like(frame)
            #apply the fft to the frame
            new_frame = applyFFT(audioBuffer, video_fps, frame, new_frame, fft_r_coeff, fft_g_coeff, fft_b_coeff, red_bias, green_bias, blue_bias)

        elif algorithm == 'diffuse':
            #convert the frame to float
            frame = frame.astype(np.float32)
            #create a new frame
            new_frame = np.zeros_like(frame)
            #diffuse the frame
            audioRMS = audioBuffer.getRMS(audioBuffer.getBuffer(int(sr / video_fps)))
            dc = audioRMS * diffusion_coeff
            print ("Diffusion coefficient: ", dc)
            print ("Audio RMS: ", audioRMS)
            new_frame = diffuseFrame(frame, dc, iterations)

        elif algorithm == 'diffuse_aulisha':
            #convert the frame to float
            frame = frame.astype(np.float32)
            #create a new frame
            new_frame = np.zeros_like(frame)
            #diffuse the frame
            audioRMS = audioBuffer.getRMS(audioBuffer.getBuffer(int(sr / video_fps)))
            dc = audioRMS * diffusion_coeff
            print ("Diffusion coefficient: ", dc)
            print ("Audio RMS: ", audioRMS)
            new_frame = diffuseFrame(frame, dc, iterations)
            #set the index back
            audioBuffer.setIndex(audioBuffer.index - int(sr / video_fps))
            #cast audio reactive light and shadow
            new_frame = castAudioLightShadow(new_frame, audioBuffer, sr, shadow_coeff, light_coeff, video_fps, lightR, lightG, lightB, shadowR, shadowG, shadowB)

        elif algorithm == 'shadow':
            #convert the frame to float
            frame = frame.astype(np.float32)
            #create a new frame
            new_frame = np.zeros_like(frame)
            audioRMS = audioBuffer.getRMS(audioBuffer.getBuffer(int(sr / video_fps)))
            sc = audioRMS * shadow_coeff
            print ("Shadow coefficient: ", sc)
            print ("Audio RMS: ", audioRMS)
            new_frame = castShadow(frame, sc)

        elif algorithm == 'fft_shadow':
            #convert the frame to float
            frame = frame.astype(np.float32)
            #create a new frame
            new_frame = np.zeros_like(frame)
            #apply the fft to the frame
            new_frame = applyFFT(audioBuffer, video_fps, frame, new_frame, fft_r_coeff, fft_g_coeff, fft_b_coeff, red_bias, green_bias, blue_bias)
            audioBuffer.setIndex(audioBuffer.index - int(sr / video_fps))
            audioRMS = audioBuffer.getRMS(audioBuffer.getBuffer(int(sr / video_fps)))
            sc = audioRMS * shadow_coeff
            print ("Shadow coefficient: ", sc)
            print ("Audio RMS: ", audioRMS)
            new_frame = castShadow(new_frame, sc)
        
        elif algorithm == 'shadow_fft':
            #convert the frame to float
            frame = frame.astype(np.float32)
            #create a new frame
            new_frame = np.zeros_like(frame)
            audioRMS = audioBuffer.getRMS(audioBuffer.getBuffer(int(sr / video_fps)))
            audioBuffer.setIndex(audioBuffer.index - int(sr / video_fps))
            sc = audioRMS * shadow_coeff
            print ("Shadow coefficient: ", sc)
            print ("Audio RMS: ", audioRMS)
            new_frame = castShadow(frame, sc)
            #apply the fft to the frame
            new_frame = applyFFT(audioBuffer, video_fps, new_frame, new_frame, fft_r_coeff, fft_g_coeff, fft_b_coeff, red_bias, green_bias, blue_bias)

        elif algorithm == 'audsha':
            #convert the frame to float
            frame = frame.astype(np.float32)
            #create a new frame
            new_frame = np.zeros_like(frame)
            #cast audio reactive shadow
            new_frame = castAudioReactiveShadow(frame, audioBuffer, sr, shadow_coeff, video_fps)

        elif algorithm == 'audsha_shadow':
            #convert the frame to float
            frame = frame.astype(np.float32)
            #create a new frame
            new_frame = np.zeros_like(frame)
            #cast audio reactive shadow
            new_frame = castAudioReactiveShadow(frame, audioBuffer, sr, shadow_coeff, video_fps)
            #cast shadow
            audioRMS = audioBuffer.getRMS(audioBuffer.getBuffer(int(sr / video_fps)))
            audioBuffer.setIndex(audioBuffer.index - int(sr / video_fps))
            sc = audioRMS * shadow_coeff
            print ("Shadow coefficient: ", sc)
            print ("Audio RMS: ", audioRMS)
            new_frame = castShadow(new_frame, sc)

        elif algorithm == 'light':
            #convert the frame to float
            frame = frame.astype(np.float32)
            #create a new frame
            new_frame = np.zeros_like(frame)
            audioRMS = audioBuffer.getRMS(audioBuffer.getBuffer(int(sr / video_fps)))
            lc = audioRMS * light_coeff
            new_frame = showLight(frame, lc)

        elif algorithm == 'light_shadow':
            #convert the frame to float
            frame = frame.astype(np.float32)
            #create a new frame
            new_frame = np.zeros_like(frame)
            audioRMS = audioBuffer.getRMS(audioBuffer.getBuffer(int(sr / video_fps)))
            lc = audioRMS * light_coeff
            new_frame = showLight(frame, lc)
            sc = audioRMS * shadow_coeff
            new_frame = castShadow(new_frame, sc)

        elif algorithm == 'audsha_light':
            #convert the frame to float
            frame = frame.astype(np.float32)
            #create a new frame
            new_frame = np.zeros_like(frame)
            #cast audio reactive shadow
            new_frame = castAudioReactiveShadow(frame, audioBuffer, sr, shadow_coeff, video_fps)
            audioRMS = audioBuffer.getRMS(audioBuffer.getBuffer(int(sr / video_fps)))
            audioBuffer.setIndex(audioBuffer.index - int(sr / video_fps))
            lc = audioRMS * light_coeff
            new_frame = showLight(new_frame, lc)

        elif algorithm == 'aulisha':
            #convert the frame to float
            frame = frame.astype(np.float32)
            #create a new frame
            new_frame = np.zeros_like(frame)
            #cast audio reactive light and shadow
            new_frame = castAudioLightShadow(frame, audioBuffer, sr, shadow_coeff, light_coeff, video_fps, lightR, lightG, lightB, shadowR, shadowG, shadowB)

        elif algorithm == 'fft_aulisha':
            #convert the frame to float
            frame = frame.astype(np.float32)
            #create a new frame
            new_frame = np.zeros_like(frame)
            #apply the fft to the frame
            new_frame = applyFFT(audioBuffer, video_fps, frame, new_frame, fft_r_coeff, fft_g_coeff, fft_b_coeff, red_bias, green_bias, blue_bias)
            #cast audio reactive light and shadow
            new_frame = castAudioLightShadow(new_frame, audioBuffer, sr, shadow_coeff, light_coeff, video_fps, lightR, lightG, lightB, shadowR, shadowG, shadowB)

        elif algorithm == 'fft_shadow':
            #convert the frame to float
            frame = frame.astype(np.float32)
            #create a new frame
            new_frame = np.zeros_like(frame)
            #apply the fft to the frame
            new_frame = applyFFT(audioBuffer, video_fps, frame, new_frame, fft_r_coeff, fft_g_coeff, fft_b_coeff, red_bias, green_bias, blue_bias)
            audioBuffer.setIndex(audioBuffer.index - int(sr / video_fps)) #set index back, because fft needs to get the same audio buffer as castShadow()
            #cast shadow
            audioRMS = audioBuffer.getRMS(audioBuffer.getBuffer(int(sr / video_fps)))
            sc = audioRMS * shadow_coeff
            new_frame = castShadow(new_frame, sc)

        elif algorithm == 'none':
            new_frame = frame

        elif algorithm == 'framefb':
            #convert the frame to float
            frame = frame.astype(np.float32)
            #get old frame
            old_frame = videoBuffer.data[videoBuffer.index - 1]
            old_frame = old_frame.astype(np.float32)
            #create a frame feedback object
            audioRMS = audioBuffer.getRMS(audioBuffer.getBuffer(int(sr / video_fps)))
            fc = audioRMS * feedback_coeff
            frameFeedback = FrameFeedback(frame, old_frame, fc, audioBuffer, sr, video_fps)
            #apply the frame feedback
            new_frame = frameFeedback.apply()

        elif algorithm == 'framefb_shadow':
            print ("algorithm: framefb_shadow")
            print ("Parameters: feedback_coeff: ", feedback_coeff, " shadow_coeff: ", shadow_coeff)
            #convert the frame to float
            frame = frame.astype(np.float32)
            #get old frame
            old_frame = videoBuffer.data[videoBuffer.index - 1]
            old_frame = old_frame.astype(np.float32)
            #create a frame feedback
            audioRMS = audioBuffer.getRMS(audioBuffer.getBuffer(int(sr / video_fps)))
            audioBuffer.setIndex(audioBuffer.index - int(sr / video_fps))
            feedback_coeff = audioRMS * feedback_coeff
            frameFeedback = FrameFeedback(frame, old_frame, feedback_coeff, audioBuffer, sr, video_fps)
            #apply the frame feedback
            new_frame = frameFeedback.apply()
            #cast shadow
            audioRMS = audioBuffer.getRMS(audioBuffer.getBuffer(int(sr / video_fps)))
            shadow_coeff = audioRMS * shadow_coeff
            new_frame = castShadow(new_frame, shadow_coeff)

        elif algorithm == 'audiocolor':
            print ("algorithm: audiocolor")
            print ("Parameters: color_coeff: ", color_coeff)
            #convert the frame to float
            frame = frame.astype(np.float32)
            #create an audio color object
            audioColor = AudioColor(frame, audioBuffer, sr, video_fps, color_coeff)
            #apply the audio color
            new_frame = audioColor.apply()

        elif algorithm == 'aulisha_audiocolor':
            print ("algorithm: aulisha_audiocolor")
            print ("Parameters: shadow_coeff: ", shadow_coeff, " light_coeff: ", light_coeff, " video_fps: ", video_fps, " lightR: ", lightR, " lightG: ", lightG, " lightB: ", lightB, " shadowR: ", shadowR, " shadowG: ", shadowG, " shadowB: ", shadowB)
            #convert the frame to float
            frame = frame.astype(np.float32)
            #create a new frame
            new_frame = np.zeros_like(frame)
            #cast audio reactive light and shadow
            new_frame = castAudioLightShadow(frame, audioBuffer, sr, shadow_coeff, light_coeff, video_fps, lightR, lightG, lightB, shadowR, shadowG, shadowB)
            audioBuffer.setIndex(audioBuffer.index - int(sr / video_fps))
            #create an audio color object
            audioColor = AudioColor(new_frame, audioBuffer, sr, video_fps, color_coeff)
            #apply the audio color
            new_frame = audioColor.apply()

        elif algorithm == 'audiocolor_aulisha':
            print ("algorithm: audiocolor_aulisha")
            print ("Parameters: color_coeff: ", color_coeff, " shadow_coeff: ", shadow_coeff, " light_coeff: ", light_coeff, " video_fps: ", video_fps, " lightR: ", lightR, " lightG: ", lightG, " lightB: ", lightB, " shadowR: ", shadowR, " shadowG: ", shadowG, " shadowB: ", shadowB)
            #convert the frame to float
            frame = frame.astype(np.float32)
            #create a new frame
            new_frame = np.zeros_like(frame)
            #create an audio color object
            audioColor = AudioColor(frame, audioBuffer, sr, video_fps, color_coeff)
            #apply the audio color
            new_frame = audioColor.apply()
            #set the audio buffer back, we need the same audio buffer for the next algorithm
            audioBuffer.setIndex(audioBuffer.index - int(sr / video_fps))
            #cast audio reactive light and shadow
            new_frame = castAudioLightShadow(new_frame, audioBuffer, sr, shadow_coeff, light_coeff, video_fps, lightR, lightG, lightB, shadowR, shadowG, shadowB)

        elif algorithm == 'audiocolor_shadow':
            print ("algorithm: audiocolor_shadow")
            print ("Parameters: color_coeff: ", color_coeff, " shadow_coeff: ", shadow_coeff)
            #convert the frame to float
            frame = frame.astype(np.float32)
            #create an audio color object
            audioColor = AudioColor(frame, audioBuffer, sr, video_fps, color_coeff)
            #apply the audio color
            new_frame = audioColor.apply()
            #return audioBuffer index back
            audioBuffer.setIndex(audioBuffer.index - int(sr / video_fps))
            #cast shadow
            audioRMS = audioBuffer.getRMS(audioBuffer.getBuffer(int(sr / video_fps)))
            audioBuffer.setIndex(audioBuffer.index - int(sr / video_fps))
            sc = audioRMS * shadow_coeff
            print ("Shadow coefficient: ", sc)
            print ("Audio RMS: ", audioRMS)
            new_frame = castShadow(new_frame, sc)

        elif algorithm == 'audiocolor_diffuse':
            print ("algorithm: audiocolor_diffuse")
            print ("Parameters: color_coeff: ", color_coeff, " diffusion_coeff: ", diffusion_coeff, " iterations: ", iterations)
            #convert the frame to float
            frame = frame.astype(np.float32)
            #create an audio color object
            audioColor = AudioColor(frame, audioBuffer, sr, video_fps, color_coeff)
            #apply the audio color
            new_frame = audioColor.apply()
            #set index back
            audioBuffer.setIndex(audioBuffer.index - int(sr / video_fps))
            #diffuse the frame
            audioRMS = audioBuffer.getRMS(audioBuffer.getBuffer(int(sr / video_fps)))
            dc = audioRMS * diffusion_coeff
            print ("Diffusion coefficient: ", dc)
            print ("Audio RMS: ", audioRMS)
            new_frame = diffuseFrame(new_frame, dc, iterations)


        elif algorithm == 'audiocolor8':
            print ("algorithm: audiocolor8")
            print ("Parameters: color_coeff: ", color_coeff)
            #convert the frame to float
            frame_in = frame.astype(np.float32)
            #create an audio color object
            audioColor8 = AudioColor8(frame_in, audioBuffer, sr, video_fps, color_coeff, grayscale)
            #apply the audio color
            new_frame = audioColor8.apply()

        elif algorithm == 'audcollisha':
            print ("algorithm: audcollisha")
            print ("Parameters: fft_r_coeff: ", fft_r_coeff, " fft_g_coeff: ", fft_g_coeff, " fft_b_coeff: ", fft_b_coeff, " shadow_coeff: ", shadow_coeff, " light_coeff: ", light_coeff, " video_fps: ", video_fps, " lightR: ", lightR, " lightG: ", lightG, " lightB: ", lightB, " shadowR: ", shadowR, " shadowG: ", shadowG, " shadowB: ", shadowB, " nolightshadow: ", nolightshadow)
            #convert the frame to float
            frame = frame.astype(np.float32)
            #create a new frame
            new_frame = np.zeros_like(frame)
            #create an AudioColorLightShadow object
            audioColorLightShadow = AudioColorLightShadow(frame, audioBuffer, sr, video_fps, color_coeff, shadow_coeff, light_coeff, lightR, lightG, lightB, shadowR, shadowG, shadowB, nolightshadow, fft_r_coeff, fft_g_coeff, fft_b_coeff)
            #apply the audio color light shadow
            new_frame = audioColorLightShadow.apply()

        elif algorithm == 'static_audiocolor8':
            print ("algorithm: static_audiocolor8")
            print ("Parameters: color_coeff: ", color_coeff)
            #convert the frame to float
            frame_in = frame.astype(np.float32)
            #create an audio color object
            audioColor8 = StaticAudioColor8(frame_in, audioBuffer, sr, video_fps, color_coeff, grayscale)
            #apply the audio color
            new_frame = audioColor8.apply()

        elif algorithm == 'static_audiocolor8_2':
            print ("algorithm: static_audiocolor8_2")
            print ("Parameters: color_coeff: ", color_coeff)
            #convert the frame to float
            frame_in = frame.astype(np.float32)
            #create an audio color object
            audioColor8 = StaticAudioColor8_2(frame_in, audioBuffer, sr, video_fps, color_coeff, grayscale)
            #apply the audio color
            new_frame = audioColor8.apply() 


        new_frame = new_frame.astype(np.uint8)

        #show the frame in pygame window
        pygame.surfarray.blit_array(screen, new_frame)
        pygame.display.flip()

        #swap axes of the frame
        new_frame = np.swapaxes(new_frame, 1, 0)
        #resize frame
        #new_frame = cv2.resize(new_frame, (height, width))
        #rgb to bgr
        new_frame = cv2.cvtColor(new_frame, cv2.COLOR_RGB2BGR)
        #write the frame to the output video
        video_output.write(new_frame)

        #check for quit event
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                video_output.release()
                pygame.quit()
                return
            
if __name__ == "__main__":
    main()

