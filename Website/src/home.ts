import { createSSRApp } from 'vue'
import HomePage from './pages/HomePage.vue'
import './styles.css'

// 接管构建时预渲染好的 HTML（server.ts）
createSSRApp(HomePage).mount('#app')
