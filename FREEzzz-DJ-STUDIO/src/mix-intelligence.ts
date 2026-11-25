import type { TrackAnalysis } from './track-analysis'

export type MixAssessment = {
  score: number
  harmonic: number
  tempo: number
  energy: number
  tempoAdjustment: number | null
  recommendation: string
  plan: { sourceStart: number; targetStart: number; bars: 4 | 8 | 16; duration: number; style: 'PHRASE BLEND' | 'EQ SWAP' | 'CUT / EFFECT' }
}

const CAMELOT_MAJOR = [8,3,10,5,12,7,2,9,4,11,6,1]
const CAMELOT_MINOR = [5,12,7,2,9,4,11,6,1,8,3,10]

function camelot(key: string | null) {
  if (!key) return null
  const match = key.match(/^([A-G](?:♯|♭)?)\s+(major|minor)$/i)
  if (!match) return null
  const pitchMap: Record<string, number> = {C:0,'C♯':1,Db:1,D:2,'D♯':3,Eb:3,E:4,F:5,'F♯':6,Gb:6,G:7,'G♯':8,Ab:8,A:9,'A♯':10,Bb:10,B:11}
  const pitch = pitchMap[match[1]]
  if (pitch === undefined) return null
  const isMajor = match[2].toLowerCase() === 'major'
  return { number: (isMajor ? CAMELOT_MAJOR : CAMELOT_MINOR)[pitch], letter: isMajor ? 'B' : 'A' }
}

function harmonicScore(a: string | null, b: string | null) {
  const left = camelot(a), right = camelot(b)
  if (!left || !right) return 45
  const distance = Math.abs(left.number - right.number)
  const wheelDistance = Math.min(distance, 12 - distance)
  if (left.number === right.number && left.letter === right.letter) return 100
  if (left.number === right.number && left.letter !== right.letter) return 94
  if (wheelDistance === 1 && left.letter === right.letter) return 88
  if (wheelDistance === 2 && left.letter === right.letter) return 68
  return 28
}

export function assessMix(a: TrackAnalysis | null, b: TrackAnalysis | null): MixAssessment | null {
  if (!a || !b || !a.bpm || !b.bpm) return null
  const ratio = a.bpm / b.bpm
  const tempoAdjustment = (ratio - 1) * 100
  const canBeatmatch = ratio >= .84 && ratio <= 1.16
  const tempo = !canBeatmatch ? 15 : Math.max(25, 100 - Math.abs(tempoAdjustment) * 3.8)
  const harmonic = harmonicScore(a.musicalKey, b.musicalKey)
  const energy = Math.max(0, 100 - Math.abs(a.energy - b.energy) * 125)
  const score = Math.round(harmonic * .5 + tempo * .3 + energy * .2)
  let recommendation = 'Use a short transition and verify phrasing by ear.'
  if (score >= 88) recommendation = 'Strong match: try a long phrase-aligned blend.'
  else if (score >= 72) recommendation = 'Good match: use EQ swap and a 8–16 bar transition.'
  else if (harmonic < 50) recommendation = 'Keys clash: use a short cut, echo-out or filtered transition.'
  else if (!canBeatmatch) recommendation = 'Tempo gap is outside the ±16% pitch range; use a breakdown or transition effect.'
  const sourceSection = [...a.sections].reverse().find(section => section.label === 'OUTRO' || section.label === 'BREAKDOWN') ?? a.sections[Math.max(0,a.sections.length-2)]
  const targetSection = b.sections.find(section => section.label === 'INTRO' || section.label === 'BUILD') ?? b.sections[0]
  const bars: 4 | 8 | 16 = score >= 85 && harmonic >= 80 && tempo >= 75 && a.downbeatConfidence >= .2 && b.downbeatConfidence >= .2 ? 16 : score >= 65 ? 8 : 4
  const alignToBar = (time: number, offset: number, interval: number) => { const bar = interval * 4; return offset + Math.max(0,Math.ceil((time-offset)/bar-.0001))*bar }
  const sourceBar = a.beatInterval ? a.beatInterval : 60/a.bpm
  const targetBeat = b.beatInterval ? b.beatInterval : 60/b.bpm
  const duration = sourceBar * 4 * bars
  const targetDuration = targetBeat * 4 * bars
  const sourceAligned = sourceSection ? alignToBar(sourceSection.start,a.downbeatOffset,sourceBar) : a.downbeatOffset
  const targetAligned = targetSection ? alignToBar(targetSection.start,b.downbeatOffset,targetBeat) : b.downbeatOffset
  const sourceStart = Math.min(sourceAligned,Math.max(0,a.duration-duration))
  const targetStart = Math.min(targetAligned,Math.max(0,b.duration-targetDuration))
  const style: MixAssessment['plan']['style'] = harmonic < 50 || !canBeatmatch ? 'CUT / EFFECT' : score >= 85 ? 'PHRASE BLEND' : 'EQ SWAP'
  return { score, harmonic, tempo: Math.round(tempo), energy: Math.round(energy), tempoAdjustment: canBeatmatch ? Number(tempoAdjustment.toFixed(1)) : null, recommendation, plan: { sourceStart, targetStart, bars, duration, style } }
}
