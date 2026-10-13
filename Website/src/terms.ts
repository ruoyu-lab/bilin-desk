import { createApp } from 'vue'
import terms from '../../App/legal/terms.md?raw'
import LegalPage from './pages/LegalPage.vue'
import './styles.css'

createApp(LegalPage, { source: terms }).mount('#app')
