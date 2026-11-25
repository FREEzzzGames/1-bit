# FREEzzz DJ STUDIO — development build 0.2

Локальная браузерная DJ-студия. Аудиофайлы остаются на устройстве пользователя.

## Реализовано в текущей ветке

- Импорт локальных MP3, WAV, OGG, FLAC, M4A и AAC (фактическое декодирование зависит от браузера).
- Две независимые деки A/B с загрузкой треков из общей библиотеки.
- Воспроизведение, пауза, позиционирование и отдельные playhead/time displays.
- Кроссфейдер с equal-power кривой и master volume.
- 3-полосный EQ на каждой деке: low / mid / high.
- Low-pass фильтр и echo send на каждой деке.
- Ручной tempo range ±16%.
- BPM sync A→B с попыткой фазового выравнивания по оценённым beat grid (эвристически).
- Cue point и 4 hot-cue pads на каждой деке; beat-grid loop 1/2/4/8 bars и выход из loop.
- Запись master output в WebM/Opus (формат зависит от MediaRecorder браузера) с экспортом файла.
- Спектральный анализ Deck A и генеративный Three.js визуализатор.

## Ограничения текущей реализации

- BPM, локально уточнённые beat markers, downbeat phase, tempo stability и музыкальные фразы оцениваются эвристически; точность зависит от жанра, синкоп и качества мастеринга. Downbeat inference опирается на басовую энергию и onset strength, это не полноценная метрическая ML-модель.
- SYNC меняет темп и корректирует фазу по beat grid, но не гарантирует идеальное совпадение downbeat/фраз.
- Tempo использует playbackRate, поэтому одновременно меняется высота тона (key lock / time-stretch ещё нет).
- Hot Cue пока хранит четыре точки в памяти текущей сессии; persistence и cue naming ещё не реализованы.
- Loop стартует на следующей оценённой границе такта; точность зависит от beat grid. Quantized loop exit пока отсутствует.
- Echo — базовая delay-секция, не полноценный FX rack.
- Анализатор считает BPM/confidence, beat grid, bar markers, 8/16-bar phrase map, RMS energy, dynamic range, spectral centroid, danceability proxy, chroma-based key estimate и waveform из PCM.
- При загрузке обеих дек появляется harmonic/tempo/energy compatibility score и phrase-aware transition plan: выбор участка outro/breakdown на A и intro/build на B, длина 4/8/16 тактов, выравнивание по downbeat. Используются Camelot wheel + BPM range. Это DSP/эвристики, не ML-модель.
- Stem separation, вокальные нарезки, beat slicing и mashup/remix arrangement ещё не реализованы. Auto Mix пока использует эвристический план и не выполняет pitch-preserving key lock.
- Waveform и arrangement map строятся по PCM-анализу; labels секций — эвристические гипотезы, а не распознанные человеком музыкальные события. Локальные beat markers корректируются по onset peak в ограниченном временном окне; tempo stability — оценка MAD интервалов.

## Запуск

Требуется Node.js 22 (или совместимая актуальная версия) и npm.

```bash
cd FREEzzz-DJ-STUDIO
npm install
npm run dev -- --host 0.0.0.0
```

Production build:

```bash
npm run typecheck
npm run build
```

Приложение работает локально в браузере. Для полноценной производительности анализ больших файлов требует достаточного объёма RAM.
