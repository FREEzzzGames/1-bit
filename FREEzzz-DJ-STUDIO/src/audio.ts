import { analyzeFile as analyzeTrackFile } from './track-analysis'
export { analyzeBuffer, analyzeFile } from './track-analysis'
export type { TrackAnalysis, TrackSection } from './track-analysis'

export type AudioState = {
  context: AudioContext
  analyser: AnalyserNode
  gain: GainNode
  source: MediaElementAudioSourceNode
}

export function connectAudio(element: HTMLAudioElement): AudioState {
  const context = new AudioContext()
  const source = context.createMediaElementSource(element)
  const analyser = context.createAnalyser()
  const gain = context.createGain()
  analyser.fftSize = 2048
  analyser.smoothingTimeConstant = 0.82
  source.connect(analyser)
  analyser.connect(gain)
  gain.connect(context.destination)
  return { context, analyser, gain, source }
}

export const analyzeFile = analyzeTrackFile

export function formatTime(seconds: number): string {
  if (!Number.isFinite(seconds)) return '00:00'
  const m = Math.floor(seconds / 60).toString().padStart(2, '0')
  const s = Math.floor(seconds % 60).toString().padStart(2, '0')
  return `${m}:${s}`
}
