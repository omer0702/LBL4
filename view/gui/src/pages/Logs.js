import React, { useState, useEffect } from 'react';
import { fetchEvents } from '../services/api';
import { 
  AlertCircle, 
  AlertTriangle, 
  Info, 
  ChevronDown, 
  ChevronUp, 
  Search, 
  Filter,
  Clock,
  FileText
} from 'lucide-react';

const SeverityBadge = ({ severity }) => {
  const styles = {
    CRITICAL: "bg-red-500/20 text-red-400 border-red-500/50",
    WARNING: "bg-yellow-500/20 text-yellow-400 border-yellow-500/50",
    INFO: "bg-blue-500/20 text-blue-400 border-blue-500/50"
  };
  return (
    <span className={`px-2 py-1 rounded border text-[10px] font-bold uppercase ${styles[severity] || styles.INFO}`}>
      {severity}
    </span>
  );
};

const LogRow = ({ event }) => {
  const [isOpen, setIsOpen] = useState(false);
  const Icon = event.severity === 'CRITICAL' ? AlertCircle : (event.severity === 'WARNING' ? AlertTriangle : Info);

  return (
    <div className="border-b border-slate-800 last:border-none">
      <div 
        onClick={() => setIsOpen(!isOpen)}
        className="flex items-center gap-4 px-6 py-4 hover:bg-slate-800/50 cursor-pointer transition-colors"
      >
        <div className={event.severity === 'CRITICAL' ? 'text-red-500' : (event.severity === 'WARNING' ? 'text-yellow-500' : 'text-blue-500')}>
          <Icon size={18} />
        </div>
        <div className="text-xs font-mono text-gray-500 w-40">
          {new Date(event.timestamp).toLocaleString()}
        </div>
        <div className="w-24">
          <SeverityBadge severity={event.severity} />
        </div>
        <div className="w-32 font-semibold text-sm text-lb-accent">
          {event.event_type}
        </div>
        <div className="flex-1 text-sm text-gray-300 truncate">
          {event.service_name || "SYSTEM"}
        </div>
        <div className="text-gray-500">
          {isOpen ? <ChevronUp size={16} /> : <ChevronDown size={16} />}
        </div>
      </div>
      
      {isOpen && (
        <div className="px-16 py-4 bg-slate-900/50 border-t border-slate-800 animate-in fade-in slide-in-from-top-1">
          <div className="mb-2">
            <h4 className="text-xs font-bold text-gray-500 uppercase mb-1">Detailed Message</h4>
            <p className="text-sm text-gray-200 leading-relaxed">{event.message}</p>
          </div>
          {event.metadata && (
            <div>
              <h4 className="text-xs font-bold text-gray-500 uppercase mb-1">Metadata</h4>
              <pre className="text-xs bg-black/30 p-2 rounded text-green-400 font-mono overflow-x-auto">
                {JSON.stringify(event.metadata, null, 2)}
              </pre>
            </div>
          )}
        </div>
      )}
    </div>
  );
};

const Logs = () => {
  const [events, setEvents] = useState([]);
  const [searchTerm, setSearchTerm] = useState("");
  const [filterSeverity, setFilterSeverity] = useState("ALL");

  const loadLogs = async () => {
    try {
      const data = await fetchEvents(30); // נשלוף יותר לוגים לצורך חיפוש/פילטר
      setEvents(data);
    } catch (e) { console.error(e); }
  };

  useEffect(() => {
    loadLogs();
    const interval = setInterval(loadLogs, 5000);
    return () => clearInterval(interval);
  }, []);

  const stats = {
    INFO: events.filter(e => e.severity === 'INFO').length,
    WARNING: events.filter(e => e.severity === 'WARNING').length,
    CRITICAL: events.filter(e => e.severity === 'CRITICAL').length,
  };

  const filteredEvents = events.filter(e => {
    const matchesSearch = e.message.toLowerCase().includes(searchTerm.toLowerCase()) || 
                         (e.service_name && e.service_name.toLowerCase().includes(searchTerm.toLowerCase()));
    const matchesSeverity = filterSeverity === "ALL" || e.severity === filterSeverity;
    return matchesSearch && matchesSeverity;
  });

  return (
    <div className="p-8">
      {/* Header & Stats Cards */}
      <div className="flex flex-col md:flex-row gap-6 mb-8 items-end">
        <div className="flex-1">
          <h1 className="text-2xl font-bold mb-2">System Events Log</h1>
          <p className="text-gray-400 text-sm">Monitor system behavior, backend registrations, and errors.</p>
        </div>
        
        <div className="flex gap-4">
          <div className="bg-lb-sidebar px-4 py-2 rounded-lg border border-slate-700 flex items-center gap-3">
            <div className="w-2 h-2 rounded-full bg-blue-500 shadow-[0_0_8px_rgba(59,130,246,0.6)]"></div>
            <span className="text-sm font-bold">{stats.INFO} INFO</span>
          </div>
          <div className="bg-lb-sidebar px-4 py-2 rounded-lg border border-slate-700 flex items-center gap-3">
            <div className="w-2 h-2 rounded-full bg-yellow-500 shadow-[0_0_8px_rgba(234,179,8,0.6)]"></div>
            <span className="text-sm font-bold">{stats.WARNING} WARNING</span>
          </div>
          <div className="bg-lb-sidebar px-4 py-2 rounded-lg border border-slate-700 flex items-center gap-3">
            <div className="w-2 h-2 rounded-full bg-red-500 shadow-[0_0_8px_rgba(239,68,68,0.6)]"></div>
            <span className="text-sm font-bold">{stats.CRITICAL} CRITICAL</span>
          </div>
        </div>
      </div>

      {/* Controls: Search & Filter */}
      <div className="bg-lb-sidebar p-4 rounded-t-xl border border-slate-700 border-b-none flex flex-wrap gap-4 items-center">
        <div className="relative flex-1 min-w-[300px]">
          <Search className="absolute left-3 top-1/2 -translate-y-1/2 text-gray-500" size={18} />
          <input 
            type="text" 
            placeholder="Search in logs or service names..." 
            className="w-full bg-slate-900 border border-slate-700 rounded-lg py-2 pl-10 pr-4 text-sm focus:outline-none focus:border-lb-accent"
            value={searchTerm}
            onChange={(e) => setSearchTerm(e.target.value)}
          />
        </div>
        <div className="flex items-center gap-2">
          <Filter size={18} className="text-gray-500" />
          <select 
            className="bg-slate-900 border border-slate-700 rounded-lg py-2 px-4 text-sm outline-none"
            value={filterSeverity}
            onChange={(e) => setFilterSeverity(e.target.value)}
          >
            <option value="ALL">All Severities</option>
            <option value="INFO">Info Only</option>
            <option value="WARNING">Warning Only</option>
            <option value="CRITICAL">Critical Only</option>
          </select>
        </div>
        <button 
          onClick={loadLogs}
          className="p-2 hover:bg-slate-700 rounded-lg text-gray-400 transition-colors"
          title="Refresh Logs"
        >
          <Clock size={18} />
        </button>
      </div>

      {/* Logs Table Area */}
      <div className="bg-lb-sidebar rounded-b-xl border border-slate-700 overflow-hidden min-h-[400px]">
        <div className="bg-slate-800/50 px-6 py-3 flex text-[10px] font-bold text-gray-500 uppercase tracking-wider">
          <div className="w-4 flex-shrink-0"></div>
          <div className="w-40 ml-4">Timestamp</div>
          <div className="w-24">Severity</div>
          <div className="w-32">Event Type</div>
          <div className="flex-1">Service / Source</div>
        </div>
        
        <div className="flex flex-col h-[calc(100vh-450px)] overflow-y-auto">
          {filteredEvents.length > 0 ? (
            filteredEvents.map(event => (
              <LogRow key={event.event_id} event={event} />
            ))
          ) : (
            <div className="flex flex-col items-center justify-center py-20 text-gray-600">
              <FileText size={48} className="mb-4 opacity-20" />
              <p>No events found matching your criteria.</p>
            </div>
          )}
        </div>
      </div>
    </div>
  );
};

export default Logs;