import { resolve } from 'path'
import inject from '@rollup/plugin-inject';

export default {
  root: resolve(__dirname, 'src'),
  build: {
    outDir: '../../data',
    emptyOutDir: true, // clear the outDir before building
    rollupOptions: {
      output: {
        assetFileNames: "[name]-[hash][extname]",
        chunkFileNames: '[name]-[hash].js',
        entryFileNames: '[name]-[hash].js'
      },
    },

  },
  plugins: [
    inject({
        $: 'jquery',
    }),
  ]
} 