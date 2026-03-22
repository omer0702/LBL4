const API_BASE_URL = "http://localhost:8000/api";

export const fetchOverview = async () => {
    const response = await fetch(`${API_BASE_URL}/overview`);
    if (!response.ok) throw new Error("Failed to fetch overview");
    return response.json();
};

export const fetchEvents = async () => {
    const response = await fetch(`${API_BASE_URL}/events`);
    if (!response.ok) throw new Error("Failed to fetch events");
    return response.json();
};

export const checkSystemHealth = async () => {
    const response = await fetch(`${API_BASE_URL}/health`);
    return response.json();
};

export const fetchServiceMetrics = async (id) => {
    const response = await fetch(`${API_BASE_URL}/services/${id}/metrics`);
    return response.json();
};

export const fetchServiceBackends = async (id) => {
    const response = await fetch(`${API_BASE_URL}/services/${id}/backends`);
    return response.json();
};