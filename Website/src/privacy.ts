import { createApp } from 'vue'
import privacy from '../../App/legal/privacy.md?raw'
import LegalPage from './pages/LegalPage.vue'
import './styles.css'

createApp(LegalPage, { source: privacy }).mount('#app')
