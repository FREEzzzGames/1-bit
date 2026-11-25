export type TrackSection = {
  start: number
  end: number
  label: 'INTRO' | 'BUILD' | 'DROP' | 'BREAKDOWN' | 'OUTRO' | 'GROOVE'
  energy: number
  confidence: number
}
export type TrackAnalysis = {
  duration: number
  bpm: number | null
  bpmConfidence: number
  beatInterval: number | null
  beatOffset: number
  beats: number[]
  bars: number[]
  phraseLengthBars: 8 | 16
  musicalKey: string | null
  keyConfidence: number
  energy: number
  dynamicRange: number
  sections: TrackSection[]
  waveform: number[]
  spectralCentroid: number
  danceability: number
  analyzedFrames: number
}

const NOTE_NAMES = ['C','C♯','D','D♯','E','F','F♯','G','G♯','A','A♯','B']
const MAJOR = [6.35,2.23,3.48,2.33,4.38,4.09,2.52,5.19,2.39,3.66,2.29,2.88]
const MINOR = [6.33,2.68,3.52,5.38,2.6,3.53,2.54,4.75,3.98,2.69,3.34,3.17]

function fft(real: Float32Array, imag: Float32Array) {
  const n = real.length
  for (let i = 1, j = 0; i < n; i++) {
    let bit = n >> 1
    for (; j & bit; bit >>= 1) j ^= bit
    j ^= bit
    if (i < j) { const r = real[i]; real[i] = real[j]; real[j] = r; const im = imag[i]; imag[i] = imag[j]; imag[j] = im }
  }
  for (let len = 2; len <= n; len <<= 1) {
    const angle = -2 * Math.PI / len
    const wr0 = Math.cos(angle), wi0 = Math.sin(angle)
    for (let i = 0; i < n; i += len) {
      let wr = 1, wi = 0
      for (let j = 0; j < len / 2; j++) {
        const u = i + j, v = u + len / 2
        const vr = real[v] * wr - imag[v] * wi
        const vi = real[v] * wi + imag[v] * wr
        real[v] = real[u] - vr; imag[v] = imag[u] - vi
        real[u] += vr; imag[u] += vi
        const next = wr * wr0 - wi * wi0
        wi = wr * wi0 + wi * wr0; wr = next
      }
    }
  }
}

function correlation(a: number[], b: number[]) {
  const am = a.reduce((s,v) => s + v, 0) / a.length
  const bm = b.reduce((s,v) => s + v, 0) / b.length
  let numerator = 0, aa = 0, bb = 0
  for (let i = 0; i < a.length; i++) {
    const x = a[i] - am, y = b[i] - bm
    numerator += x * y; aa += x * x; bb += y * y
  }
  return numerator / Math.sqrt(Math.max(1e-12, aa * bb))
}

function estimateKey(chroma: number[]) {
  const total = chroma.reduce((s,v) => s + v, 0)
  if (!total) return { musicalKey: null, keyConfidence: 0 }
  const normalized = chroma.map(v => v / total)
  let best = -Infinity, second = -Infinity, name: string | null = null
  for (let root = 0; root < 12; root++) {
    for (const [profile, mode] of [[MAJOR,'major'],[MINOR,'minor']] as const) {
      const rotated = profile.map((_,i) => profile[(i - root + 12) % 12])
      const score = correlation(normalized, rotated)
      if (score > best) { second = best; best = score; name = `${NOTE_NAMES[root]} ${mode}` }
      else if (score > second) second = score
    }
  }
  return { musicalKey: name, keyConfidence: Math.max(0, Math.min(1, (best - second) * 2.5)) }
}

export function analyzeBuffer(buffer: AudioBuffer): TrackAnalysis {
  const duration = buffer.duration
  const sourceRate = buffer.sampleRate
  const stride = Math.max(1, Math.floor(sourceRate / 11025))
  const sampleRate = sourceRate / stride
  const channels = Array.from({length: buffer.numberOfChannels}, (_,i) => buffer.getChannelData(i))
  const length = Math.floor(buffer.length / stride)
  const maxFrames = 6000
  const frameSize = 2048
  const hop = Math.max(512, Math.ceil(length / (maxFrames * 512)) * 512)
  const mono = new Float32Array(length)
  for (let i = 0; i < length; i++) {
    const sourceIndex = i * stride
    let value = 0
    for (const channel of channels) value += channel[sourceIndex] || 0
    mono[i] = value / channels.length
  }
  const frameRate = sampleRate / hop
  const rms: number[] = [], flux: number[] = [], centroid: number[] = []
  const chroma = new Array(12).fill(0)
  const waveform: number[] = []
  let previous = new Float32Array(frameSize / 2 + 1)
  const real = new Float32Array(frameSize), imag = new Float32Array(frameSize)
  let centroidWeighted = 0, centroidWeight = 0
  for (let start = 0; start + frameSize < length; start += hop) {
    let square = 0, crossing = 0
    for (let j = 0; j < frameSize; j++) {
      const sample = mono[start + j]
      square += sample * sample
      if (j && (sample >= 0) !== (mono[start + j - 1] >= 0)) crossing++
      real[j] = sample * (0.5 - 0.5 * Math.cos(2 * Math.PI * j / (frameSize - 1)))
      imag[j] = 0
    }
    const level = Math.sqrt(square / frameSize)
    rms.push(level)
    fft(real, imag)
    let positiveFlux = 0, magnitudeSum = 0, weightedFrequency = 0
    const magnitudes = new Float32Array(frameSize / 2 + 1)
    for (let bin = 1; bin < magnitudes.length; bin++) {
      const magnitude = Math.hypot(real[bin], imag[bin])
      magnitudes[bin] = magnitude
      positiveFlux += Math.max(0, magnitude - previous[bin])
      previous[bin] = magnitude
      const frequency = bin * sampleRate / frameSize
      magnitudeSum += magnitude
      weightedFrequency += magnitude * frequency
      if (frequency >= 55 && frequency <= 2000) {
        const midi = Math.round(69 + 12 * Math.log2(frequency / 440))
        chroma[((midi % 12) + 12) % 12] += magnitude
      }
    }
    flux.push(positiveFlux / Math.max(1, magnitudeSum))
    const center = weightedFrequency / Math.max(1e-9, magnitudeSum)
    centroid.push(center)
    centroidWeighted += center * magnitudeSum; centroidWeight += magnitudeSum
    if (start % (hop * Math.max(1, Math.floor(rms.length / 180))) === 0) waveform.push(level)
    void crossing
  }
  if (!rms.length) throw new Error('Audio is too short to analyze')
  const mean = (values: number[]) => values.reduce((s,v) => s + v, 0) / values.length
  const sortedRms = [...rms].sort((a,b) => a-b)
  const q10 = sortedRms[Math.floor(sortedRms.length * .1)] || 0
  const q90 = sortedRms[Math.floor(sortedRms.length * .9)] || 0
  const avgRms = mean(rms)
  const normalizedFlux = flux.map((v,i) => Math.max(0, v - mean(flux) * .65) * (i ? 1 : 0))
  const minLag = Math.max(1, Math.floor(frameRate * 60 / 180))
  const maxLag = Math.min(normalizedFlux.length - 2, Math.ceil(frameRate * 60 / 65))
  let bestLag = 0, bestScore = -Infinity, secondScore = -Infinity
  const scores: number[] = []
  for (let lag = minLag; lag <= maxLag; lag++) {
    let score = 0, normA = 0, normB = 0
    for (let i = lag; i < normalizedFlux.length; i++) {
      const a = normalizedFlux[i], b = normalizedFlux[i-lag]
      score += a*b; normA += a*a; normB += b*b
    }
    score /= Math.sqrt(Math.max(1e-12, normA*normB))
    scores.push(score)
    if (score > bestScore) { secondScore = bestScore; bestScore = score; bestLag = lag }
    else if (score > secondScore) secondScore = score
  }
  let bpm = bestLag ? 60 * frameRate / bestLag : null
  if (bpm && bpm < 85) bpm *= 2
  if (bpm && bpm > 175) bpm /= 2
  const beatInterval = bpm ? 60 / bpm : null
  const phaseFrames = bestLag ? Math.max(1, Math.round(frameRate * 60 / bpm!)) : 0
  let beatOffset = 0
  if (phaseFrames) {
    let phaseBest = -Infinity
    for (let phase = 0; phase < phaseFrames; phase++) {
      let score = 0
      for (let i = phase; i < normalizedFlux.length; i += phaseFrames) score += normalizedFlux[i]
      if (score > phaseBest) { phaseBest = score; beatOffset = phase / frameRate }
    }
  }
  const beatConfidence = Math.max(0, Math.min(1, (bestScore - Math.max(0, secondScore)) * 3.5))
  const beats: number[] = []
  if (beatInterval) for (let t = beatOffset; t < duration; t += beatInterval) beats.push(Number(t.toFixed(4)))
  const bars: number[] = []
  if (beatInterval) for (let i = 0; i < beats.length; i += 4) bars.push(beats[i])
  const phraseLengthBars: 8 | 16 = duration > 240 ? 16 : 8
  const phraseSeconds = (beatInterval || .5) * 4 * phraseLengthBars
  const sections: TrackSection[] = []
  for (let start = 0; start < duration; start += phraseSeconds) {
    const from = Math.floor(start * frameRate), to = Math.min(rms.length, Math.ceil((start + phraseSeconds) * frameRate))
    if (to <= from) continue
    const localRms = mean(rms.slice(from,to))
    const localFlux = mean(normalizedFlux.slice(from,to))
    const energy = Math.max(0, Math.min(1, localRms / Math.max(.001, q90)))
    const previousEnergy = sections[sections.length-1]?.energy ?? energy
    let label: TrackSection['label'] = 'GROOVE'
    if (start < phraseSeconds * 1.5 && energy < .72) label = 'INTRO'
    else if (energy > previousEnergy * 1.22 && localFlux > mean(normalizedFlux) * 1.1) label = 'DROP'
    else if (localFlux > mean(normalizedFlux) * 1.35) label = 'BUILD'
    else if (energy < .42) label = 'BREAKDOWN'
    else if (duration - start < phraseSeconds * 1.5) label = 'OUTRO'
    sections.push({start:Number(start.toFixed(2)),end:Number(Math.min(duration,start+phraseSeconds).toFixed(2)),label,energy:Number(energy.toFixed(3)),confidence:Number(Math.min(.9,.35+Math.abs(energy-previousEnergy)*1.4+localFlux).toFixed(2))})
  }
  const key = estimateKey(chroma)
  const danceability = Math.max(0, Math.min(1, beatConfidence * .65 + Math.min(1, mean(normalizedFlux)*18) * .35))
  return {
    duration, bpm: bpm ? Number(bpm.toFixed(2)) : null, bpmConfidence: Number(beatConfidence.toFixed(3)),
    beatInterval, beatOffset, beats, bars, phraseLengthBars, ...key,
    energy: Number(Math.min(1,avgRms / Math.max(.001,q90)).toFixed(3)),
    dynamicRange: Number((20 * Math.log10(Math.max(1e-5,q90) / Math.max(1e-5,q10))).toFixed(1)),
    sections, waveform, spectralCentroid: Number((centroidWeighted / Math.max(1e-9,centroidWeight)).toFixed(1)),
    danceability: Number(danceability.toFixed(3)), analyzedFrames: rms.length
  }
}

export async function analyzeFile(file: File): Promise<TrackAnalysis> {
  const context = new AudioContext()
  try {
    const buffer = await context.decodeAudioData(await file.arrayBuffer())
    return analyzeBuffer(buffer)
  } finally {
    await context.close()
  }
}
