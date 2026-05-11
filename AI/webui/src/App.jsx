import { useEffect } from 'react'
import useStore from './store'
import Sidebar from './components/Sidebar'
import ChatView from './components/ChatView'
import AgentsPanel from './components/AgentsPanel'

export default function App() {
  const { loadModels, loadConversations, loadAgents, activePanel, error, clearError } = useStore()

  useEffect(() => {
    loadModels().catch(() => {})
    loadConversations().catch(() => {})
    loadAgents().catch(() => {})
  }, [])

  return (
    <div className="app">
      <Sidebar />
      <main className="main">
        {activePanel === 'chat'   && <ChatView />}
        {activePanel === 'agents' && <AgentsPanel />}
      </main>

      {error && (
        <div className="error-toast">
          <span style={{ flex: 1 }}>{error}</span>
          <button onClick={clearError}>✕</button>
        </div>
      )}
    </div>
  )
}
