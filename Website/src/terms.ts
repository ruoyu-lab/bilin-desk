import { createSSRApp } from 'vue'
import terms from '../../App/legal/terms.md?raw'
import LegalPage from './pages/LegalPage.vue'
import './styles.css'

// 接管构建时预渲染好的 HTML（server.ts）
createSSRApp(LegalPage, { source: terms }).mount('#app')
