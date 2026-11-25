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
- BPM-based sync A→B, если оценённые BPM попадают в диапазон доступной коррекции.
- Cue point и 4 hot-cue pads на каждой деке; beat-length loop 1/2/4/8 bars и выход из loop.
- Запись master output в WebM/Opus (формат зависит от MediaRecorder браузера) с экспортом файла.
- Спектральный анализ Deck A и генеративный Three.js визуализатор.

## Ограничения текущей реализации

- BPM, beat grid и музыкальные фразы оцениваются эвристически; точность зависит от жанра, синкоп и качества мастеринга. Downbeat classifier пока отсутствует.
- SYNC выравнивает только темп по BPM, не гарантирует фазовое совпадение ударов.
- Tempo использует playbackRate, поэтому одновременно меняется высота тона (key lock / time-stretch ещё нет).
- Cue реализован как одна точка на деку; hot-cue banks пока нет.
- Loop начинает повтор с текущей позиции, длительность рассчитывается по BPM; quantize пока отсутствует.
- Echo — базовая delay-секция, не полноценный FX rack.
- Анализатор уже считает BPM/confidence, beat grid, bar markers, 8/16-bar phrase map, энергию, динамический диапазон, spectral centroid, danceability proxy и chroma-based key estimate. Это локальная DSP-оценка, не ML-модель.
- Stem separation, вокальные нарезки, beat slicing, mashup/remix arrangement и AI DJ transitions ещё не реализованы.
- Waveform и arrangement map строятся по PCM-анализу; labels секций — эвристические гипотезы, а не распознанные человеком музыкальные события.

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
