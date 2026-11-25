export type DeckId = 'A' | 'B'

type DeckGraph = {
  element: HTMLAudioElement
  source: MediaElementAudioSourceNode
  analyser: AnalyserNode
  low: BiquadFilterNode
  mid: BiquadFilterNode
  high: BiquadFilterNode
  tone: BiquadFilterNode
  gain: GainNode
  delay: DelayNode
  delayFeedback: GainNode
  delayWet: GainNode
  cue: number
  hotCues: (number | null)[]
  loop: { start: number; end: number } | null
  bpm: number | null
  tempo: number
  onTime: () => void
}

export class DJEngine {
  readonly context = new AudioContext()
  readonly master = this.context.createGain()
  readonly decks: Record<DeckId, DeckGraph>
  private crossfader = 0
  private masterVolume = 0.85
  private recorder: MediaRecorder | null = null
  private recordingChunks: BlobPart[] = []
  private recordingDestination = this.context.createMediaStreamDestination()

  constructor(a: HTMLAudioElement, b: HTMLAudioElement) {
    this.master.gain.value = this.masterVolume
    this.master.connect(this.context.destination)
    this.master.connect(this.recordingDestination)
    this.decks = { A: this.createDeck(a), B: this.createDeck(b) }
    this.applyCrossfader()
  }

  private createDeck(element: HTMLAudioElement): DeckGraph {
    const source = this.context.createMediaElementSource(element)
    const low = this.context.createBiquadFilter()
    low.type = 'lowshelf'; low.frequency.value = 120; low.gain.value = 0
    const mid = this.context.createBiquadFilter()
    mid.type = 'peaking'; mid.frequency.value = 1000; mid.Q.value = 0.8; mid.gain.value = 0
    const high = this.context.createBiquadFilter()
    high.type = 'highshelf'; high.frequency.value = 8000; high.gain.value = 0
    const tone = this.context.createBiquadFilter()
    tone.type = 'lowpass'; tone.frequency.value = 20000; tone.Q.value = 0.7
    const analyser = this.context.createAnalyser()
    analyser.fftSize = 2048
    analyser.smoothingTimeConstant = 0.82
    const gain = this.context.createGain()
    const delay = this.context.createDelay(1.5)
    delay.delayTime.value = 0.32
    const delayFeedback = this.context.createGain()
    delayFeedback.gain.value = 0.28
    const delayWet = this.context.createGain()
    delayWet.gain.value = 0
    source.connect(low).connect(mid).connect(high).connect(tone)
    tone.connect(analyser).connect(gain).connect(this.master)
    tone.connect(delay).connect(delayWet).connect(this.master)
    delay.connect(delayFeedback).connect(delay)
    const graph: DeckGraph = {
      element, source, analyser, low, mid, high, tone, gain, delay, delayFeedback, delayWet,
      cue: 0, hotCues: [null, null, null, null], loop: null, bpm: null, tempo: 0, onTime: () => undefined,
    }
    graph.onTime = () => {
      if (graph.loop && element.currentTime >= graph.loop.end) element.currentTime = graph.loop.start
    }
    element.addEventListener('timeupdate', graph.onTime)
    return graph
  }

  async resume() {
    if (this.context.state !== 'running') await this.context.resume()
  }

  load(id: DeckId, url: string, bpm: number | null) {
    const deck = this.decks[id]
    deck.element.pause()
    deck.element.src = url
    deck.element.load()
    deck.bpm = bpm
    deck.cue = 0
    deck.hotCues = [null, null, null, null]
    deck.loop = null
    deck.tempo = 0
    deck.element.playbackRate = 1
  }

  async toggle(id: DeckId) {
    await this.resume()
    const element = this.decks[id].element
    if (element.paused) await element.play()
    else element.pause()
  }

  setTempo(id: DeckId, percent: number) {
    const deck = this.decks[id]
    deck.tempo = Math.max(-16, Math.min(16, percent))
    deck.element.playbackRate = 1 + deck.tempo / 100
  }

  sync(sourceId: DeckId, targetId: DeckId) {
    const source = this.decks[sourceId]
    const target = this.decks[targetId]
    if (!source.bpm || !target.bpm) return false
    const targetRate = source.bpm / target.bpm
    if (targetRate < 0.84 || targetRate > 1.16) return false
    this.setTempo(targetId, (targetRate - 1) * 100)
    return true
  }

  setEQ(id: DeckId, band: 'low' | 'mid' | 'high', value: number) {
    this.decks[id][band].gain.setTargetAtTime(value, this.context.currentTime, 0.015)
  }

  setFilter(id: DeckId, value: number) {
    const min = 180
    const max = 20000
    const frequency = min * Math.pow(max / min, Math.max(0, Math.min(1, value)))
    this.decks[id].tone.frequency.setTargetAtTime(frequency, this.context.currentTime, 0.02)
  }

  setEcho(id: DeckId, enabled: boolean) {
    this.decks[id].delayWet.gain.setTargetAtTime(enabled ? 0.24 : 0, this.context.currentTime, 0.025)
  }

  setChannelGain(id: DeckId, value: number) {
    this.decks[id].gain.gain.setTargetAtTime(Math.max(0, Math.min(1.25, value)), this.context.currentTime, 0.015)
  }

  setCrossfader(value: number) {
    this.crossfader = Math.max(-1, Math.min(1, value))
    this.applyCrossfader()
  }

  private applyCrossfader() {
    const angle = (this.crossfader + 1) * Math.PI / 4
    this.setChannelGain('A', Math.cos(angle))
    this.setChannelGain('B', Math.sin(angle))
  }

  setMasterVolume(value: number) {
    this.masterVolume = Math.max(0, Math.min(1, value))
    this.master.gain.setTargetAtTime(this.masterVolume, this.context.currentTime, 0.015)
  }

  setCue(id: DeckId) {
    this.decks[id].cue = this.decks[id].element.currentTime
  }

  returnToCue(id: DeckId) {
    const deck = this.decks[id]
    deck.element.currentTime = deck.cue
    deck.element.pause()
  }

  async pressHotCue(id: DeckId, index: number) {
    const deck = this.decks[id]
    if (index < 0 || index >= deck.hotCues.length) return
    const cue = deck.hotCues[index]
    if (cue === null) {
      deck.hotCues[index] = deck.element.currentTime
      return
    }
    await this.resume()
    deck.element.currentTime = cue
    await deck.element.play()
  }

  startRecording() {
    if (typeof MediaRecorder === 'undefined') throw new Error('MediaRecorder is not supported')
    if (this.recorder && this.recorder.state !== 'inactive') throw new Error('Recording is already active')
    this.recordingChunks = []
    this.recorder = new MediaRecorder(this.recordingDestination.stream)
    this.recorder.ondataavailable = event => {
      if (event.data.size > 0) this.recordingChunks.push(event.data)
    }
    this.recorder.start(1000)
  }

  stopRecording(): Promise<Blob> {
    const recorder = this.recorder
    if (!recorder || recorder.state === 'inactive') return Promise.reject(new Error('No active recording'))
    return new Promise((resolve, reject) => {
      recorder.onerror = () => reject(new Error('Recording failed'))
      recorder.onstop = () => resolve(new Blob(this.recordingChunks, { type: recorder.mimeType || 'audio/webm' }))
      recorder.stop()
    })
  }

  setLoop(id: DeckId, bars: 1 | 2 | 4 | 8) {
    const deck = this.decks[id]
    const bpm = deck.bpm ? deck.bpm * (1 + deck.tempo / 100) : 120
    const duration = (60 / bpm) * 4 * bars
    const start = deck.element.currentTime
    deck.loop = { start, end: Math.min(deck.element.duration || start + duration, start + duration) }
  }

  clearLoop(id: DeckId) {
    this.decks[id].loop = null
  }

  seek(id: DeckId, seconds: number) {
    const element = this.decks[id].element
    element.currentTime = Math.max(0, Math.min(element.duration || 0, seconds))
  }

  getAnalyser(id: DeckId) { return this.decks[id].analyser }

  dispose() {
    for (const deck of Object.values(this.decks)) {
      deck.element.pause()
      deck.element.removeEventListener('timeupdate', deck.onTime)
      deck.source.disconnect()
      deck.low.disconnect(); deck.mid.disconnect(); deck.high.disconnect(); deck.tone.disconnect()
      deck.analyser.disconnect(); deck.gain.disconnect()
      deck.delay.disconnect(); deck.delayFeedback.disconnect(); deck.delayWet.disconnect()
    }
    this.master.disconnect()
    this.recordingDestination.disconnect()
    void this.context.close()
  }
}
