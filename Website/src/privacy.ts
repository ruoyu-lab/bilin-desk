import { createSSRApp } from 'vue'
import privacy from '../../App/legal/privacy.md?raw'
import LegalPage from './pages/LegalPage.vue'
import './styles.css'

// 接管构建时预渲染好的 HTML（server.ts）
createSSRApp(LegalPage, { source: privacy }).mount('#app')
