import htmlTemplate from 'rollup-plugin-generate-html-template';
import serve from 'rollup-plugin-serve';
import postcss from 'rollup-plugin-postcss';
import { uglify } from 'rollup-plugin-uglify';
import cssnano from 'cssnano';
import cssnext from 'postcss-cssnext';
import { resolve } from 'path';

const isProduction = process.env.NODE_ENV === 'production';

const Plugins = isProduction ? [uglify()] : [serve({
  contentBase: ['dist'],
  port: 3000,
})];

export default {
  input: "web/app.js",
  output: {
    format: "esm",
    file: "dist/app.js",
  },
  watch: {
    include: 'web/**'
  },
  plugins: [
    htmlTemplate({
      template: './web/index.html',
      target: './dist/index.html',
    }),
    postcss({
      plugins: [cssnext, cssnano],
      extract: resolve(__dirname, `./dist/style.css`) // 输出路径
    }),
    serve({
      contentBase: ['dist'],
      port: 3000,
    })
  ],
};