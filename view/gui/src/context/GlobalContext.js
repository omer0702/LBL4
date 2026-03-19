import React, { createContext, useState, useContext, useEffect } from 'react';
import { fetchOverview } from '../services/api';

const GlobalContext = createContext();

export const GlobalProvider = ({ children }) => {
    const [services, setServices] = useState([]);
    const [loading, setLoading] = useState(true);
    const [globalStats, setGlobalStats] = useState({
        totalPps: 0,
        activeServices: 0,
        systemStatus: 'Online'
    });

    const refreshData = async () => {
        try {
            const data = await fetchOverview();
            setServices(data);
            
            // חישוב נתונים גלובליים
            const totalPps = data.reduce((acc, s) => acc + s.total_pps, 0);
            const activeServices = data.filter(s => s.active_backends > 0).length;
            
            setGlobalStats({
                totalPps: totalPps.toFixed(1),
                activeServices: activeServices,
                systemStatus: 'Online'
            });
            setLoading(false);
        } catch (error) {
            console.error("Polling error:", error);
            setGlobalStats(prev => ({ ...prev, systemStatus: 'Error' }));
        }
    };

    useEffect(() => {
        refreshData(); // ריצה ראשונית
        const interval = setInterval(refreshData, 5000); // כל 5 שניות
        return () => clearInterval(interval);
    }, []);

    return (
        <GlobalContext.Provider value={{ services, globalStats, loading }}>
            {children}
        </GlobalContext.Provider>
    );
};

export const useGlobal = () => useContext(GlobalContext);