// Vite plugin for src/modes.json, the single catalogue of modes shared by the
// web UI and the iOS app:
// - fails the build if the catalogue disagrees with the TX or RX firmware
// - copies it into the build output, so the TX also serves /modes.json
import { readFileSync, existsSync } from 'fs'
import { fileURLToPath } from 'url'

const here = (path) => fileURLToPath(new URL(path, import.meta.url))
const CATALOGUE = here('./src/modes.json')
const TX_MAIN = here('../src/main.cpp')
const RX_MAIN = here('../../RX/src/main.cpp')

function catalogueModes(catalogue) {
  const modes = []
  for (const effect of catalogue.effects) {
    for (const entry of [...effect.modes, ...(effect.twoColour || [])]) {
      modes.push(entry)
    }
  }
  return modes
}

export function validateCatalogue(catalogue, txSource, rxSource) {
  const problems = []
  const entries = catalogueModes(catalogue)
  const numbers = entries.map((e) => e.mode)

  // every entry is labelled, by name or by known colours
  for (const entry of entries) {
    if (!entry.name && !(entry.colours && entry.colours.length)) {
      problems.push(`mode ${entry.mode} has neither a name nor colours`)
    }
    for (const colour of entry.colours || []) {
      if (!catalogue.colours[colour]) problems.push(`mode ${entry.mode} uses unknown colour "${colour}"`)
    }
  }

  // no mode appears twice, including the actions
  const all = [...numbers, ...Object.values(catalogue.actions)]
  for (const n of new Set(all)) {
    if (all.filter((m) => m === n).length > 1) problems.push(`mode ${n} is listed more than once`)
  }

  // the catalogue version matches what the TX reports
  const version = txSource.match(/const int CATALOGUE_VERSION\s*=\s*(\d+)/)
  if (!version) {
    problems.push('CATALOGUE_VERSION not found in TX/src/main.cpp')
  } else if (Number(version[1]) !== catalogue.catalogue) {
    problems.push(`catalogue is ${catalogue.catalogue} but TX CATALOGUE_VERSION is ${version[1]}`)
  }

  // the Auto list matches the TX's autoModes[], and only picks catalogue modes
  const autoBlock = txSource.match(/const int autoModes\[\d*\]\s*=\s*\{([^}]*)\}/)
  if (!autoBlock) {
    problems.push('autoModes[] not found in TX/src/main.cpp')
  } else {
    const txAuto = autoBlock[1].split(',').map((s) => Number(s.trim())).filter((n) => !Number.isNaN(n))
    const same = txAuto.length === catalogue.auto.length && txAuto.every((n) => catalogue.auto.includes(n))
    if (!same) problems.push(`auto is [${catalogue.auto}] but TX autoModes[] is [${txAuto}]`)
  }
  for (const n of catalogue.auto) {
    if (!numbers.includes(n)) problems.push(`auto mode ${n} is not in the catalogue`)
  }

  // every mode the UI can send is one the RX knows
  if (rxSource !== null) {
    const rxCases = new Set([...rxSource.matchAll(/case\s+(-?\d+)\s*:/g)].map((m) => Number(m[1])))
    for (const n of all) {
      if (!rxCases.has(n)) problems.push(`mode ${n} has no case in RX/src/main.cpp`)
    }
  }

  return problems
}

export default function cataloguePlugin() {
  let catalogue
  return {
    name: 'drum-lights-catalogue',
    buildStart() {
      this.addWatchFile(CATALOGUE)
      catalogue = JSON.parse(readFileSync(CATALOGUE, 'utf8'))
      const txSource = readFileSync(TX_MAIN, 'utf8')
      const rxSource = existsSync(RX_MAIN) ? readFileSync(RX_MAIN, 'utf8') : null
      if (rxSource === null) this.warn('RX/src/main.cpp not found; skipping the RX mode check')

      const problems = validateCatalogue(catalogue, txSource, rxSource)
      if (problems.length) {
        this.error(`src/modes.json does not match the firmware:\n  - ${problems.join('\n  - ')}`)
      }
    },
    generateBundle() {
      this.emitFile({ type: 'asset', fileName: 'modes.json', source: JSON.stringify(catalogue) })
    },
  }
}
