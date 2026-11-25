import { useEffect, useRef, useState } from 'react'
import { Activity, AudioLines, Disc3, FolderPlus, Headphones, Library, Pause, Play, Radio, Repeat2, SkipBack, Sparkles, Volume2, Zap } from 'lucide-react'
import Visualizer from './Visualizer'
import { analyzeFile, formatTime } from './audio'
import { DJEngine, type DeckId } from './dj-engine'

type Track = { id: string; file: File; url: string; name: string; duration: number; bpm: number | null }
type DeckState = { trackId: string | null; playing: boolean; tempo: number; loop: boolean; echo: boolean; filter: number; low: number; mid: number; high: number; time: number; duration: number }
const initialDeck = (): DeckState => ({ trackId: null, playing: false, tempo: 0, loop: false, echo: false, filter: 1, low: 0, mid: 0, high: 0, time: 0, duration: 0 })
const modes = ['PARTICLES', 'FRACTAL', 'WAVE', 'TORUS']

function Deck({ id, state, track, engine, onChange, onPlay, onCue, onReturnCue, onLoop, onEcho, onEQ, onFilter, onTempo, onSync }: {
  id: DeckId; state: DeckState; track: Track | null; engine: DJEngine | null
  onChange: (patch: Partial<DeckState>) => void; onPlay: () => void; onCue: () => void; onReturnCue: () => void
  onLoop: (bars: 1 | 2 | 4 | 8) => void; onEcho: () => void; onEQ: (band: 'low' | 'mid' | 'high', value: number) => void
  onFilter: (value: number) => void; onTempo: (value: number) => void; onSync: () => void
}) {
  return <section className="deck panel">
    <div className="deck-top"><div className="deck-name"><span className="deck-letter">{id}</span><div><small>PLAYER DECK</small><b>{track?.name ?? 'NO TRACK LOADED'}</b></div></div><span className={state.playing ? 'live-tag' : 'idle-tag'}>{state.playing ? '● ON AIR' : '○ STANDBY'}</span></div>
    <div className="deck-wave"><div className="wave-grid"/><div className="wave-center"/><div className="waveform" style={{ transform: `scaleX(${track ? 1 : 0.08})` }}/><span className="playhead" style={{ left: `${state.duration ? state.time / state.duration * 100 : 0}%` }}/></div>
    <div className="deck-time"><span>{formatTime(state.time)}</span><span>{track?.bpm ? `${track.bpm} BPM · ANALYZED` : 'BPM —'}</span><span>{formatTime(state.duration)}</span></div>
    <input aria-label={`Deck ${id} seek`} className="seekbar" type="range" min="0" max={state.duration || 1} step=".05" value={Math.min(state.time, state.duration || 1)} onChange={e => engine?.seek(id, Number(e.target.value))} disabled={!track}/>
    <div className="deck-transport"><button className="control-button" onClick={onReturnCue} disabled={!track}><SkipBack size={15}/> CUE</button><button className="deck-play" onClick={onPlay} disabled={!track}>{state.playing ? <Pause/> : <Play/>}</button><button className="control-button" onClick={onCue} disabled={!track}>SET CUE</button><button className={state.echo ? 'control-button active-fx' : 'control-button'} onClick={onEcho} disabled={!track}>ECHO</button></div>
    <div className="tempo-row"><label>TEMPO <b>{state.tempo > 0 ? '+' : ''}{state.tempo.toFixed(1)}%</b></label><input type="range" min="-16" max="16" step=".1" value={state.tempo} onChange={e => onTempo(Number(e.target.value))}/><button className="sync-button" onClick={onSync} disabled={!track}>SYNC</button></div>
    <div className="eq-grid">{(['low','mid','high'] as const).map((band) => <label key={band}><span>{band.toUpperCase()}</span><input type="range" min="-18" max="12" step="1" value={state[band]} onChange={e => onEQ(band, Number(e.target.value))}/><b>{state[band]} dB</b></label>)}</div>
    <div className="filter-row"><span>FILTER</span><input type="range" min="0" max="1" step=".01" value={state.filter} onChange={e => onFilter(Number(e.target.value))}/><span>{state.filter < .5 ? 'LOW PASS' : 'OPEN'}</span></div>
    <div className="loop-row"><span><Repeat2 size={14}/> LOOP</span>{([1,2,4,8] as const).map(n => <button key={n} className={state.loop ? 'loop-active' : ''} onClick={() => onLoop(n)} disabled={!track}>{n} BAR</button>)}<button onClick={() => { engine?.clearLoop(id); onChange({ loop: false }) }} disabled={!state.loop}>EXIT</button></div>
  </section>
}

export default function App() {
  const [tracks, setTracks] = useState<Track[]>([])
  const [decks, setDecks] = useState<Record<DeckId, DeckState>>({ A: initialDeck(), B: initialDeck() })
  const [crossfader, setCrossfader] = useState(0)
  const [masterVolume, setMasterVolume] = useState(.85)
  const [mode, setMode] = useState('PARTICLES')
  const [intensity, setIntensity] = useState(1)
  const [analyser, setAnalyser] = useState<AnalyserNode | null>(null)
  const [status, setStatus] = useState('READY · LOCAL MODE')
  const [busy, setBusy] = useState(false)
  const audioA = useRef<HTMLAudioElement>(null)
  const audioB = useRef<HTMLAudioElement>(null)
  const engine = useRef<DJEngine | null>(null)

  useEffect(() => {
    if (!audioA.current || !audioB.current) return
    const dj = new DJEngine(audioA.current, audioB.current)
    engine.current = dj
    setAnalyser(dj.getAnalyser('A'))
    const timer = window.setInterval(() => {
      setDecks(previous => {
        const next = { ...previous }
        for (const id of ['A', 'B'] as const) {
          const element = dj.decks[id].element
          next[id] = { ...previous[id], time: element.currentTime || 0, duration: Number.isFinite(element.duration) ? element.duration : 0, playing: !element.paused }
        }
        return next
      })
    }, 120)
    return () => { window.clearInterval(timer); dj.dispose(); engine.current = null }
  }, [])

  useEffect(() => () => tracks.forEach(track => URL.revokeObjectURL(track.url)), [tracks])

  const patchDeck = (id: DeckId, patch: Partial<DeckState>) => setDecks(previous => ({ ...previous, [id]: { ...previous[id], ...patch } }))
  const addFiles = async (list: FileList | null) => {
    if (!list || busy) return
    const files = Array.from(list).filter(file => file.type.startsWith('audio/') || /\.(mp3|wav|ogg|flac|m4a|aac)$/i.test(file.name))
    setBusy(true)
    for (const file of files) {
      setStatus(`ANALYZING · ${file.name}`)
      const url = URL.createObjectURL(file)
      let analysis = { duration: 0, bpm: null as number | null }
      try { analysis = await analyzeFile(file) } catch { setStatus('ANALYSIS FAILED · TRACK STILL AVAILABLE') }
      const track = { id: crypto.randomUUID(), file, url, name: file.name.replace(/\.[^.]+$/, ''), ...analysis }
      setTracks(previous => [...previous, track])
    }
    setBusy(false)
    setStatus('LIBRARY READY')
  }

  const loadDeck = (id: DeckId, track: Track) => {
    engine.current?.load(id, track.url, track.bpm)
    patchDeck(id, { ...initialDeck(), trackId: track.id, duration: track.duration })
    setStatus(`LOADED · DECK ${id}`)
  }
  const getTrack = (id: DeckId) => tracks.find(track => track.id === decks[id].trackId) ?? null
  const play = async (id: DeckId) => {
    try { await engine.current?.toggle(id); setStatus(`DECK ${id} · TRANSPORT`) }
    catch { setStatus('PLAYBACK ERROR · CHECK AUDIO FORMAT') }
  }
  const setTempo = (id: DeckId, value: number) => { engine.current?.setTempo(id, value); patchDeck(id, { tempo: value }) }
  const setEQ = (id: DeckId, band: 'low' | 'mid' | 'high', value: number) => { engine.current?.setEQ(id, band, value); patchDeck(id, { [band]: value }) }
  const setFilter = (id: DeckId, value: number) => { engine.current?.setFilter(id, value); patchDeck(id, { filter: value }) }
  const toggleEcho = (id: DeckId) => { const enabled = !decks[id].echo; engine.current?.setEcho(id, enabled); patchDeck(id, { echo: enabled }) }
  const loop = (id: DeckId, bars: 1 | 2 | 4 | 8) => { if (decks[id].loop) { engine.current?.clearLoop(id); patchDeck(id, { loop: false }) } else { engine.current?.setLoop(id, bars); patchDeck(id, { loop: true }) } }
  const syncB = () => { const ok = engine.current?.sync('A', 'B'); setStatus(ok ? 'SYNC · B LOCKED TO A TEMPO' : 'SYNC UNAVAILABLE · CHECK BPM RANGE') }

  return <main className="app-shell">
    <header className="topbar"><div className="brand"><div className="brand-mark"><AudioLines/></div><div><strong>FREEzzz<span>DJ</span></strong><small>LOCAL MIXING & REMIX STUDIO</small></div></div><div className="system-state"><i/> {status}</div><div className="top-actions"><span className="local-pill">● OFFLINE FIRST</span><Headphones size={19}/></div></header>
    <section className="workspace">
      <aside className="sidebar panel"><div className="section-heading"><span><Library/> MUSIC LIBRARY</span><label>{String(tracks.length).padStart(2,'0')}</label></div><label className="upload-zone"><FolderPlus/><b>{busy ? 'ANALYZING…' : 'IMPORT TRACKS'}</b><small>MP3 · WAV · OGG · FLAC · M4A</small><input type="file" accept="audio/*,.mp3,.flac,.m4a" multiple onChange={e => { void addFiles(e.target.files); e.currentTarget.value = '' }}/></label><div className="track-list">{!tracks.length && <div className="empty-library">Your crate is empty.<br/>Import local music to load Deck A or B.</div>}{tracks.map((track, index) => <div className="track-row" key={track.id}><span className="track-index">{String(index + 1).padStart(2,'0')}</span><span className="track-info"><b title={track.name}>{track.name}</b><small>{track.bpm ? `${track.bpm} BPM` : 'BPM —'} · {formatTime(track.duration)}</small><span className="load-actions"><button onClick={() => loadDeck('A', track)}>LOAD A</button><button onClick={() => loadDeck('B', track)}>LOAD B</button></span></span><Disc3 size={16}/></div>)}</div><div className="sidebar-foot"><span>LOCAL FILES ONLY</span><span>v0.2 DEV</span></div></aside>
      <section className="main-column">
        <div className="mixer-header"><div><div className="eyebrow">DUAL DECK PERFORMANCE SYSTEM <span>● CORE ACTIVE</span></div><h1>DJ <em>CONSOLE</em></h1></div><div className="render-tag">WEB AUDIO / REALTIME</div></div>
        <div className="decks-grid">
          <Deck id="A" state={decks.A} track={getTrack('A')} engine={engine.current} onChange={p => patchDeck('A', p)} onPlay={() => void play('A')} onCue={() => engine.current?.setCue('A')} onReturnCue={() => engine.current?.returnToCue('A')} onLoop={n => loop('A', n)} onEcho={() => toggleEcho('A')} onEQ={(b,v) => setEQ('A',b,v)} onFilter={v => setFilter('A',v)} onTempo={v => setTempo('A',v)} onSync={syncB}/>
          <Deck id="B" state={decks.B} track={getTrack('B')} engine={engine.current} onChange={p => patchDeck('B', p)} onPlay={() => void play('B')} onCue={() => engine.current?.setCue('B')} onReturnCue={() => engine.current?.returnToCue('B')} onLoop={n => loop('B', n)} onEcho={() => toggleEcho('B')} onEQ={(b,v) => setEQ('B',b,v)} onFilter={v => setFilter('B',v)} onTempo={v => setTempo('B',v)} onSync={syncB}/>
        </div>
        <section className="panel master-mixer"><div className="master-title"><span><SlidersHorizontalIcon/> MASTER MIXER</span><span>2 CHANNELS · STEREO OUT</span></div><div className="crossfader-block"><label>DECK A</label><input aria-label="Crossfader" type="range" min="-1" max="1" step=".01" value={crossfader} onChange={e => { const value = Number(e.target.value); setCrossfader(value); engine.current?.setCrossfader(value) }}/><label>DECK B</label></div><div className="master-bottom"><span><Activity size={14}/> MASTER OUTPUT</span><label><Volume2 size={16}/><input type="range" min="0" max="1" step=".01" value={masterVolume} onChange={e => { const value = Number(e.target.value); setMasterVolume(value); engine.current?.setMasterVolume(value) }}/></label><button className="sync-button" onClick={syncB}>SYNC B ← A</button></div></section>
        <section className="hero panel"><div className="hero-head"><div><div className="eyebrow">GENERATIVE VISUAL ENGINE <span>● LIVE</span></div><h2>VISUAL <em>LAB</em></h2></div><div className="render-tag">WEBGL / REALTIME</div></div><Visualizer analyser={analyser} mode={mode} intensity={intensity}/><div className="visual-overlay"><span>MODE // {mode}</span><span>INPUT // {decks.A.playing || decks.B.playing ? 'ACTIVE' : 'STANDBY'}</span></div><div className="visual-controls"><div className="preset-group">{modes.map(m => <button key={m} onClick={() => setMode(m)} className={mode === m ? 'active' : ''}>{m}</button>)}</div><label className="intensity-control"><span>REACTIVITY</span><input type="range" min=".2" max="2.5" step=".1" value={intensity} onChange={e => setIntensity(Number(e.target.value))}/><b>{intensity.toFixed(1)}×</b></label></div></section>
      </section>
      <aside className="rightbar"><div className="panel director"><div className="section-heading"><span><Sparkles/> PERFORMANCE</span><span className="coming">LIVE</span></div><div className="director-orb"><Zap/></div><h3>DJ ENGINE ONLINE</h3><p>Two independent decks, real-time EQ, tempo control, beat-length loops, echo send and equal-power crossfader.</p><div className="feature-list"><span>✓ DUAL DECK TRANSPORT</span><span>✓ 3-BAND CHANNEL EQ</span><span>✓ TEMPO MATCH / SYNC</span><span>✓ CUE & LOOP CONTROL</span><span>✓ LOW-PASS FILTER + ECHO</span></div></div><div className="panel roadmap"><div className="section-heading"><span>NEXT MODULES</span><span>02 / 04</span></div><div className="road-item done"><i/> Dual-deck mixer core</div><div className="road-item"><i/> Beatgrid + phrase detection</div><div className="road-item"><i/> Stem separation + remix</div><div className="road-item"><i/> Auto transitions + recording</div><p className="muted-note">Tempo sync currently uses BPM ratio. Playback-rate changes also change pitch; key lock is not implemented yet.</p></div></aside>
    </section>
    <footer className="footer"><span>FREEzzz SYSTEMS <b>×</b> GENERATIVE AUDIOVISUAL</span><span>LOCAL PROCESSING · NO CLOUD</span></footer>
    <audio ref={audioA} preload="metadata"/><audio ref={audioB} preload="metadata"/>
  </main>
}

function SlidersHorizontalIcon() {
  return <span className="slider-glyph">☷</span>
}
