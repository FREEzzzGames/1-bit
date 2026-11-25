import type { TrackAnalysis } from './track-analysis'

export type MixAssessment = {
  score: number
  harmonic: number
  tempo: number
  energy: number
  tempoAdjustment: number | null
  recommendation: string
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
  return { score, harmonic, tempo: Math.round(tempo), energy: Math.round(energy), tempoAdjustment: canBeatmatch ? Number(tempoAdjustment.toFixed(1)) : null, recommendation }
}
