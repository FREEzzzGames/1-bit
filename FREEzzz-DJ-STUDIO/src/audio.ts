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

export function estimateBpm(buffer: AudioBuffer): number | null {
  // Lightweight offline onset-envelope autocorrelation estimate.
  const data = buffer.getChannelData(0)
  const hop = 512
  const envelope: number[] = []
  for (let i = 0; i < data.length; i += hop) {
    let energy = 0
    const end = Math.min(i + hop, data.length)
    for (let j = i; j < end; j++) energy += data[j] * data[j]
    envelope.push(Math.sqrt(energy / Math.max(1, end - i)))
  }
  const mean = envelope.reduce((a, b) => a + b, 0) / envelope.length
  const onset = envelope.map((v, i) => Math.max(0, v - (envelope[i - 1] ?? v) - mean * 0.03))
  const rate = buffer.sampleRate / hop
  const minLag = Math.floor(rate * 60 / 180)
  const maxLag = Math.floor(rate * 60 / 65)
  let bestLag = 0, best = -Infinity
  for (let lag = minLag; lag <= maxLag; lag++) {
    let score = 0
    for (let i = lag; i < onset.length; i++) score += onset[i] * onset[i - lag]
    if (score > best) { best = score; bestLag = lag }
  }
  if (!bestLag || best <= 0) return null
  let bpm = 60 * rate / bestLag
  while (bpm < 80) bpm *= 2
  while (bpm > 160) bpm /= 2
  return Math.round(bpm)
}

export async function analyzeFile(file: File): Promise<{ duration: number; bpm: number | null }> {
  const context = new AudioContext()
  try {
    const buffer = await context.decodeAudioData(await file.arrayBuffer())
    return { duration: buffer.duration, bpm: estimateBpm(buffer) }
  } finally {
    await context.close()
  }
}

export function formatTime(seconds: number): string {
  if (!Number.isFinite(seconds)) return '00:00'
  const m = Math.floor(seconds / 60).toString().padStart(2, '0')
  const s = Math.floor(seconds % 60).toString().padStart(2, '0')
  return `${m}:${s}`
}